/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */
#include "patch_text.h"
#include "klog.h" // IWYU pragma: keep
#include "linux/cpumask.h"
#include "linux/gfp.h" // IWYU pragma: keep
#include "linux/uaccess.h"
#include "linux/stop_machine.h"
#include "asm/cacheflush.h"
#include "asm-generic/fixmap.h"

#include "pte.h"

// This function appears in 5.14:
// https://github.com/torvalds/linux/commit/fade9c2c6ee2baea7df8e6059b3f143c681e5ce4#diff-fc9ef24572e183c6c049b5ae8029762159787f8669d909452bdf40db748f94a7L52
// https://github.com/torvalds/linux/commit/814b186079cd54d3fe3b6b8ab539cbd44705ef9d#diff-fc9ef24572e183c6c049b5ae8029762159787f8669d909452bdf40db748f94a7R53
// However, it's backport to android13-5.10 but not to android12-5.10.
// https://cs.android.com/android/_/android/kernel/common/+/6d9f07d8f1ffc310a6877153fe882f35ae380799
// So we need to grep kernel source code to detect which one to use.
#if KSU_NEW_DCACHE_FLUSH
#define ksu_flush_dcache(start, sz)                                            \
    ({                                                                         \
        unsigned long __start = (start);                                       \
        unsigned long __end = __start + (sz);                                  \
        dcache_clean_inval_poc(__start, __end);                                \
    })
#define ksu_flush_icache(start, end) caches_clean_inval_pou
#else
#define ksu_flush_dcache(start, sz) __flush_dcache_area((void *)start, sz)
#define ksu_flush_icache(start, end) __flush_icache_range
#endif

struct patch_text_info {
    void *dst;
    void *src;
    size_t len;
    atomic_t cpu_count;
    int flags;
};

// Implementation of arbitrary kernel address modification.
// We could certainly modify the PTE of the target address to make it writable,
// but this would violate the protection mechanisms of some vendor components
// (such as MTK's MKP, see ^1) at higher EL levels. Fortunately, the kernel's
// `aarch64_insn_write` function works fine, which I believe is achieved by
// modifying memory using fixmaps (MKP's kernel module reports the fixmap address
// to its hypervisor, which might be used to determine whether such memory
// modification is "normal" kernel behavior, see ^1). However, we cannot use the
// `aarch64_insn_write` function directly. First, it can only write 4 bytes
// at a time. Secondly, there's a bug in modifying the kernel's rodata (in our
// case, syscall table). The `patch_map` function uses `vmalloc_to_page` to
// obtain the target's physical address, but `vmalloc_to_page` doesn't handle
// huge page mapping correctly (before version 5.13, see ^2).
// Therefore, we need to obtain the target's physical address and use `fixmap` to
// map and poke it manually. Currently, no patch_lock is held, since I think it's
// not a big problem because we are in stop_machine.
// ^1: https://github.com/NothingOSS/android_kernel_device_modules_6.1_nothing_mt6878/blob/957dac185efe46cbf6336b0fff9516d84c8cd78f/drivers/misc/mediatek/mkp/mkp_main.c#L29
// ^2: https://github.com/torvalds/linux/commit/c0eb315ad9719e41ce44708455cc69df7ac9f3f8
static int ksu_patch_text_nosync(void *dst, void *src, size_t len, int flags)
{
    pr_info("patch dst=0x%lx src=0x%lx len=%ld\n", (unsigned long)dst,
            (unsigned long)src, len);

    unsigned long p = (unsigned long)dst;
    int ret;

    unsigned long phy = phys_from_virt(p);
    if (!phy) {
        ret = -ENOENT;
        pr_err("failed to found phy addr for patch dst addr 0x%lx\n", p);
        goto err;
    }
    pr_info("phy addr for patch 0x%lx: 0x%lx\n", p, phy);

    void *map = set_fixmap_offset(FIX_TEXT_POKE0, phy);
    pr_info("fixmap addr for patch 0x%lx: 0x%lx\n", p, (unsigned long)map);

    ret = (int)copy_to_kernel_nofault(map, src, len);

    clear_fixmap(FIX_TEXT_POKE0);

    if (!ret) {
        if (flags & KSU_PATCH_TEXT_FLUSH_ICACHE)
            ksu_flush_icache((uintptr_t)dst, (uintptr_t)dst + len);
        if (flags & KSU_PATCH_TEXT_FLUSH_DCACHE)
            ksu_flush_dcache(dst, len);
    }

err:
    pr_info("patch result=%d\n", ret);
    return ret;
}

static int ksu_patch_text_cb(void *arg)
{
    struct patch_text_info *pp = arg;
    void *dst = pp->dst, *src = pp->src;
    size_t len = pp->len;
    int flags = pp->flags;

    int ret = 0;

    /* The last CPU becomes master */
    if (atomic_inc_return(&pp->cpu_count) == num_online_cpus()) {
        ret = ksu_patch_text_nosync(dst, src, len, flags);
        /* Notify other processors with an additional increment. */
        atomic_inc(&pp->cpu_count);
    } else {
        while (atomic_read(&pp->cpu_count) <= num_online_cpus())
            cpu_relax();
        isb();
    }

    return ret;
}

int ksu_patch_text(void *dst, void *src, size_t len, int flags)
{
    struct patch_text_info info = {
        .dst = dst,
        .src = src,
        .len = len,
        .cpu_count = ATOMIC_INIT(0),
        .flags = flags,
    };

    return stop_machine(ksu_patch_text_cb, &info, cpu_online_mask);
}
