/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/version.h>
#include <linux/cpu.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/pgtable.h>
#include <linux/stop_machine.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/fixmap.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
#include <linux/unaligned.h>
#else
#include <asm/unaligned.h>
#endif

#include "infra/symbol_resolver.h"
#include "../patch_memory.h"
#include "insn.h"

static unsigned long leaf_phys(unsigned long value, unsigned long addr, unsigned long mask)
{
    return PFN_PHYS(pte_pfn(__pte(value))) + (addr & ~mask);
}

/* Kernel rodata is not core_kernel_text(). Walking init_mm also handles the
 * large leaf mappings used by Sv39/Sv48/Sv57 without vmalloc_to_page(rodata).
 */
unsigned long phys_from_virt(unsigned long addr, int *err)
{
    pgd_t *pgd = pgd_offset_k(addr);
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    *err = 0;
    if (pgd_none(*pgd))
        goto missing;
    if (pgd_val(*pgd) & _PAGE_LEAF)
        return leaf_phys(pgd_val(*pgd), addr, PGDIR_MASK);
    if (pgd_bad(*pgd))
        goto missing;
    p4d = p4d_offset(pgd, addr);
    if (p4d_none(*p4d))
        goto missing;
    if (p4d_val(*p4d) & _PAGE_LEAF)
        return leaf_phys(p4d_val(*p4d), addr, P4D_MASK);
    if (p4d_bad(*p4d))
        goto missing;
    pud = pud_offset(p4d, addr);
    if (pud_none(*pud))
        goto missing;
    if (pud_val(*pud) & _PAGE_LEAF)
        return leaf_phys(pud_val(*pud), addr, PUD_MASK);
    if (pud_bad(*pud))
        goto missing;
    pmd = pmd_offset(pud, addr);
    if (pmd_none(*pmd))
        goto missing;
    if (pmd_val(*pmd) & _PAGE_LEAF)
        return leaf_phys(pmd_val(*pmd), addr, PMD_MASK);
    if (pmd_bad(*pmd))
        goto missing;
    pte = pte_offset_kernel(pmd, addr);
    if (!pte || !pte_present(*pte))
        goto missing;
#ifdef CONFIG_RISCV_ISA_SVNAPOT
    if (pte_napot(*pte)) {
        pte_t entry = *pte;
        return leaf_phys(pte_val(entry), addr, napot_cont_mask(napot_cont_order(entry)));
    }
#endif
    return leaf_phys(pte_val(*pte), addr, PAGE_MASK);
missing:
    *err = -ENOENT;
    return 0;
}

struct patch_info {
    void *dst;
    const void *src;
    size_t len;
    int flags;
    int result;
    atomic_t arrived;
};

static int write_patch(struct patch_info *info)
{
    unsigned long addr = (unsigned long)info->dst;
    const u8 *src = info->src;
    size_t remaining = info->len;

    while (remaining) {
        int err;
        unsigned long phys = phys_from_virt(addr, &err);
        size_t len = min_t(size_t, remaining, PAGE_SIZE - offset_in_page(addr));
        void *alias;

        if (err)
            return err;
        alias = (void *)set_fixmap_offset(FIX_TEXT_POKE0, phys);
        err = copy_to_kernel_nofault(alias, src, len);
        clear_fixmap(FIX_TEXT_POKE0);
        if (err)
            return err;
        addr += len;
        src += len;
        remaining -= len;
    }
    return 0;
}

static int patch_cpu(void *data)
{
    struct patch_info *info = data;

    if (atomic_inc_return(&info->arrived) == num_online_cpus()) {
        info->result = write_patch(info);
        atomic_inc_return_release(&info->arrived);
    } else {
        while (atomic_read_acquire(&info->arrived) <= num_online_cpus())
            cpu_relax();
    }
    /* CPU data writes are coherent. Do not invoke remote fence IPIs while
     * stop_machine holds the other CPUs: each CPU performs its own fence.i.
     */
    if (info->flags & KSU_PATCH_TEXT_FLUSH_ICACHE)
        local_flush_icache_all();
    return 0;
}

int ksu_patch_text(void *dst, void *src, size_t len, int flags)
{
    struct mutex *text_lock = (struct mutex *)find_kernel_symbol_exact("text_mutex");
    unsigned long end;
    int ret;
    struct patch_info info = {
        .dst = dst,
        .src = src,
        .len = len,
        .flags = flags,
        .arrived = ATOMIC_INIT(0),
    };

    if (!len)
        return 0;
    if (!dst || !src || check_add_overflow((unsigned long)dst, len, &end))
        return -EINVAL;
    if (!text_lock)
        return -ENOENT;
    cpus_read_lock();
    mutex_lock(text_lock);
    ret = stop_machine_cpuslocked(patch_cpu, &info, cpu_online_mask);
    mutex_unlock(text_lock);
    cpus_read_unlock();
    return ret ? ret : info.result;
}

void *scan_call_to(void *start, size_t size, void *target)
{
    const u8 *code = start;
    size_t offset = 0;

    if (!start || !target)
        return NULL;
    while (size - offset >= 2) {
        unsigned int len = ksu_riscv_insn_length(get_unaligned_le16(code + offset));
        unsigned long dest, pc = (unsigned long)(code + offset);
        u32 insn;

        if (!len || size - offset < len)
            break;
        if (len == 4) {
            insn = get_unaligned_le32(code + offset);
            if (ksu_riscv_jal_target(insn, pc, &dest) && dest == (unsigned long)target)
                return (void *)(code + offset);
            if (size - offset >= 6) {
                u32 next = get_unaligned_le16(code + offset + 4);
                unsigned int next_len = ksu_riscv_insn_length(next);
                if (next_len == 4 && size - offset >= 8)
                    next = get_unaligned_le32(code + offset + 4);
                else if (next_len != 2)
                    next_len = 0;
                if (ksu_riscv_auipc_call_target(insn, next, next_len, pc, &dest) && dest == (unsigned long)target)
                    return (void *)(code + offset);
            }
        }
        offset += len;
    }
    return NULL;
}
