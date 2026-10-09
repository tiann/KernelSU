/* SPDX-License-Identifier: GPL-2.0-only */

#include <linux/jump_label.h>
#include <linux/version.h>

#include "compat/jailbreak.h"
#include "infra/symbol_resolver.h"
#include "klog.h"

DEFINE_STATIC_KEY_FALSE(ksu_rkp_key);

void rkp_jb_init(void)
{
    // Symbol presence marks an RKP-capable kernel. It does not prove the
    // protection is active for a given page or that every patch API is
    // accepted by the hypervisor.
    if (find_kernel_symbol_exact("rkp_started")) {
        static_branch_enable(&ksu_rkp_key);
        pr_info("Samsung RKP detected\n");
    }
}

#ifdef __aarch64__

#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/moduleloader.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/set_memory.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
#include <linux/execmem.h>
#define ksu_exec_alloc(sz) execmem_alloc(EXECMEM_MODULE_TEXT, (sz))
#define ksu_exec_free(p) execmem_free(p)
#else
#define ksu_exec_alloc(sz) module_alloc(sz)
#define ksu_exec_free(p) vfree(p)
#endif
#include "hook/patch_memory.h"

struct ksu_branch_site {
    void *fn_addr;
    u32 displaced_insn;
    int stub_idx;
    bool active;
    bool prepared;
};

#define KSU_MAX_BRANCH_SITES 16
/* Stub layout: [kcfi_hash][displaced_insn][B fn+4][pad]
 * Callable entry is stub+4; KCFI reads (entry-4) = stub[0] = kcfi_hash. */
#define KSU_STUB_SIZE 16

static struct ksu_branch_site ksu_sites[KSU_MAX_BRANCH_SITES];
static int ksu_site_count;
static DEFINE_MUTEX(ksu_site_lock);
static void *ksu_stub_page;
static int ksu_stub_next;

static u32 ksu_encode_b(void *from, void *to)
{
    s64 imm26 = ((s64)(unsigned long)to - (s64)(unsigned long)from) >> 2;

    if (imm26 < -(1 << 25) || imm26 > (1 << 25) - 1)
        return 0;
    return 0x14000000u | (u32)(imm26 & 0x3FFFFFFu);
}

/* BTI c landing instruction. An indirect branch to the patched entry must
 * still find a valid landing pad after the original bti is displaced, so
 * bti entries are rejected instead of hooked. */
#define KSU_INSN_BTI_C 0xD503245Fu

static bool ksu_insn_is_bti(u32 insn)
{
    return insn == KSU_INSN_BTI_C;
}

static bool ksu_insn_is_pc_relative(u32 insn)
{
    return ((insn & 0x1F000000u) == 0x10000000u) || /* ADR/ADRP */
           ((insn & 0x3B000000u) == 0x18000000u) || /* LDR literal */
           ((insn & 0x3B000000u) == 0x58000000u) ||
           ((insn & 0x7C000000u) == 0x14000000u) || /* B / BL */
           ((insn & 0xFE000000u) == 0x54000000u) || /* B.cond / BC.cond */
           ((insn & 0x7E000000u) == 0x34000000u) || /* CBZ / CBNZ */
           ((insn & 0x7E000000u) == 0x36000000u); /* TBZ / TBNZ */
}

static struct ksu_branch_site *ksu_find_site(void *fn)
{
    int i;

    for (i = 0; i < ksu_site_count; i++)
        if (ksu_sites[i].fn_addr == fn)
            return &ksu_sites[i];
    return NULL;
}

static int ksu_write_stub(void *stub, u32 kcfi_hash, u32 displaced, u32 back)
{
    int ret;

    ret = aarch64_insn_write((u8 *)stub + 0, kcfi_hash);
    if (ret)
        return ret;
    ret = aarch64_insn_write((u8 *)stub + 4, displaced);
    if (ret)
        return ret;
    ret = aarch64_insn_write((u8 *)stub + 8, back);
    if (ret)
        return ret;

    // The stub becomes callable right after the entry branch lands, so its
    // words must be visible to the icache before that. The flush API split
    // matches patch_memory.c.
#if KSU_NEW_DCACHE_FLUSH
    caches_clean_inval_pou((uintptr_t)stub, (uintptr_t)stub + KSU_STUB_SIZE);
#else
    __flush_icache_range((uintptr_t)stub, (uintptr_t)stub + KSU_STUB_SIZE);
#endif
    return 0;
}

// Prepare (but do not activate) a branch site for the function stored in
// *slot. The stub is written and callable; no kernel text is changed yet.
// Used so callers can publish call-through addresses before activation.
int ksu_rkp_patch_slot(void **slot, void *replacement)
{
    void *current_fn, *new_fn;
    struct ksu_branch_site *site;
    u32 displaced_insn, back_b;
    int ret;
    void *stub;

    current_fn = READ_ONCE(*slot);
    new_fn = replacement;

    mutex_lock(&ksu_site_lock);

    site = ksu_find_site(current_fn);
    if (site && site->active) {
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: %pS already hooked\n", current_fn);
        return -EALREADY;
    }

    if (current_fn == new_fn) {
        mutex_unlock(&ksu_site_lock);
        return 0;
    }

    displaced_insn = *(u32 *)current_fn;
    if (ksu_insn_is_pc_relative(displaced_insn)) {
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: %pS: first insn is PC-relative, cannot install\n", current_fn);
        return -EINVAL;
    }
    if (ksu_insn_is_bti(displaced_insn)) {
        // Indirect callers land on the entry instruction; replacing a bti
        // with a plain B would fault them before reaching the replacement.
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: %pS has a BTI entry, unsupported\n", current_fn);
        return -EINVAL;
    }

    if (!site) {
        if (ksu_site_count >= KSU_MAX_BRANCH_SITES) {
            mutex_unlock(&ksu_site_lock);
            pr_err("ksu: branch site table full\n");
            return -ENOSPC;
        }

        if (!ksu_stub_page) {
            ksu_stub_page = ksu_exec_alloc(PAGE_SIZE);
            if (!ksu_stub_page) {
                mutex_unlock(&ksu_site_lock);
                return -ENOMEM;
            }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
            ret = set_memory_rox((unsigned long)ksu_stub_page, 1);
#else
            // module_alloc() memory is non-executable here; make it read-only
            // AND executable or the first call-through into the stub faults.
            ret = set_memory_ro((unsigned long)ksu_stub_page, 1);
            if (!ret)
                ret = set_memory_x((unsigned long)ksu_stub_page, 1);
#endif
            if (ret) {
                ksu_exec_free(ksu_stub_page);
                ksu_stub_page = NULL;
                mutex_unlock(&ksu_site_lock);
                return ret;
            }
        }

        if (ksu_stub_next >= (int)(PAGE_SIZE / KSU_STUB_SIZE)) {
            mutex_unlock(&ksu_site_lock);
            pr_err("ksu: stub page full\n");
            return -ENOSPC;
        }

        site = &ksu_sites[ksu_site_count++];
        site->fn_addr = current_fn;
        site->stub_idx = ksu_stub_next++;
        site->active = false;
        site->prepared = false;
    }

    stub = ksu_stub_page + site->stub_idx * KSU_STUB_SIZE;
    back_b = ksu_encode_b((u8 *)stub + 8, (u8 *)current_fn + 4);
    if (!back_b) {
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: stub -> fn+4 branch out of range at %pS\n", current_fn);
        return -ERANGE;
    }

    /* The stub is prepared before activation so callers can publish its
     * callable address first. Skipped when this record already carries a
     * valid stub for the same function. */
    if (!site->prepared || site->displaced_insn != displaced_insn) {
        /* Stub page is RO+X; the words go in via the native instruction
         * writer before the branch activates. The KCFI hash is copied from
         * fn-4 so indirect callers pass the check. */
        ret = ksu_write_stub(stub, *(u32 *)((u8 *)current_fn - 4), displaced_insn, back_b);
        if (ret) {
            mutex_unlock(&ksu_site_lock);
            pr_err("ksu: stub write failed at %pS: %d\n", current_fn, ret);
            return ret;
        }
        site->displaced_insn = displaced_insn;
    }
    site->prepared = true;

    mutex_unlock(&ksu_site_lock);
    return 0;
}

// Activate a prepared site: install the entry branch for @replacement.
// The stub must already exist (ksu_compat_prepare_slot) so call-throughs
// are published before the branch can run.
int ksu_rkp_activate_slot(void **slot, void *replacement)
{
    void *current_fn, *new_fn;
    struct ksu_branch_site *site;
    u32 branch_b;
    int ret;

    current_fn = READ_ONCE(*slot);
    new_fn = replacement;

    mutex_lock(&ksu_site_lock);

    site = ksu_find_site(current_fn);
    if (!site || !site->prepared) {
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: %pS has no prepared site\n", current_fn);
        return -EINVAL;
    }
    if (site->active) {
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: %pS already hooked\n", current_fn);
        return -EALREADY;
    }

    branch_b = ksu_encode_b(current_fn, new_fn);
    if (!branch_b) {
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: %pS -> %pS branch out of range\n", current_fn, new_fn);
        return -ERANGE;
    }

    // One live reference per hooked site keeps the module text (and the stub
    // page) alive while kernel functions branch into it. Released on restore
    // or after the exit restore succeeds.
    if (!try_module_get(THIS_MODULE)) {
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: module is dying, cannot hook %pS\n", current_fn);
        return -EBUSY;
    }

    // aarch64_insn_patch_text runs stop_machine_cpuslocked, which requires
    // the CPU-hotplug read side held across the patch.
    cpus_read_lock();
    ret = aarch64_insn_patch_text(&current_fn, &branch_b, 1);
    cpus_read_unlock();
    if (ret) {
        module_put(THIS_MODULE);
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: branch install failed at %pS: %d\n", current_fn, ret);
        return ret;
    }

    site->active = true;
    pr_info("ksu: branch site installed: %pS -> %pS (stub[%d])\n", current_fn, new_fn, site->stub_idx);

    mutex_unlock(&ksu_site_lock);
    return 0;
}

// Restore a prepared/active site back to its original instruction.
int ksu_rkp_restore_slot(void *original)
{
    struct ksu_branch_site *site;
    u32 displaced_insn;
    int ret;

    mutex_lock(&ksu_site_lock);
    site = ksu_find_site(original);
    if (!site || !site->active) {
        mutex_unlock(&ksu_site_lock);
        return 0;
    }

    displaced_insn = site->displaced_insn;
    cpus_read_lock();
    ret = aarch64_insn_patch_text(&original, &displaced_insn, 1);
    cpus_read_unlock();
    if (ret) {
        mutex_unlock(&ksu_site_lock);
        pr_err("ksu: branch restore failed at %pS: %d\n", original, ret);
        return ret;
    }
    site->active = false;
    pr_info("ksu: branch site removed at %pS\n", original);
    mutex_unlock(&ksu_site_lock);
    module_put(THIS_MODULE);
    return 0;
}

void ksu_rkp_branch_exit(void)
{
    int i;

    mutex_lock(&ksu_site_lock);
    for (i = 0; i < ksu_site_count; i++) {
        if (ksu_sites[i].active) {
            void *fn = ksu_sites[i].fn_addr;
            u32 insn = ksu_sites[i].displaced_insn;

            cpus_read_lock();
            if (aarch64_insn_patch_text(&fn, &insn, 1)) {
                cpus_read_unlock();
                pr_err("ksu: exit restore failed at %pS; branch remains into module text\n", fn);
                continue;
            }
            cpus_read_unlock();
            ksu_sites[i].active = false;
            module_put(THIS_MODULE);
        }
    }
    mutex_unlock(&ksu_site_lock);
}

void *ksu_rkp_get_stub(void *fn)
{
    struct ksu_branch_site *site;
    void *result = NULL;

    mutex_lock(&ksu_site_lock);
    site = ksu_find_site(fn);
    if (site && site->prepared && ksu_stub_page)
        result = ksu_stub_page + site->stub_idx * KSU_STUB_SIZE + 4;
    mutex_unlock(&ksu_site_lock);
    return result;
}

void rkp_jb_exit(void)
{
    ksu_rkp_branch_exit();
    static_branch_disable(&ksu_rkp_key);
}

#else /* !__aarch64__ */

void rkp_jb_exit(void)
{
    static_branch_disable(&ksu_rkp_key);
}

int ksu_rkp_patch_slot(void **slot, void *replacement)
{
    return -ENOSYS;
}
int ksu_rkp_activate_slot(void **slot, void *replacement)
{
    return -ENOSYS;
}
int ksu_rkp_restore_slot(void *original)
{
    return -ENOSYS;
}
void *ksu_rkp_get_stub(void *fn)
{
    return NULL;
}

#endif /* __aarch64__ */
