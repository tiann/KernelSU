/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */

#ifdef __x86_64__

#include <linux/cache.h> 
#include "../patch_memory.h"
#include "../../klog.h" // IWYU pragma: keep
#include <linux/cpumask.h>
#include <linux/gfp.h> // IWYU pragma: keep
#include <linux/uaccess.h>
#include <linux/stop_machine.h>
#include <asm/cacheflush.h>
#include <asm-generic/fixmap.h>

#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/fixmap.h>

// --- Architecture-specific Page Table to Physical Address translation ---
#define KSU_P4D_TO_PHYS(p4d) ((unsigned long)p4d_pfn(p4d) << PAGE_SHIFT)
#define KSU_PUD_TO_PHYS(pud) ((unsigned long)pud_pfn(pud) << PAGE_SHIFT)
#define KSU_PMD_TO_PHYS(pmd) ((unsigned long)pmd_pfn(pmd) << PAGE_SHIFT)
#define KSU_PTE_TO_PHYS(pte) ((unsigned long)pte_pfn(pte) << PAGE_SHIFT)

// Translate a kernel virtual address to a physical address by walking the
// init_mm page tables.
unsigned long phys_from_virt(unsigned long addr, int *err)
{
    struct mm_struct *mm = &init_mm;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    *err = 0;

    pgd = pgd_offset(mm, addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        goto fail;
    pr_debug("pgd of 0x%lx p=0x%lx v=0x%lx", addr, (uintptr_t)pgd,
             (uintptr_t)pgd_val(*pgd));

    p4d = p4d_offset(pgd, addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
        goto fail;
    pr_debug("p4d of 0x%lx p=0x%lx v=0x%lx", addr, (uintptr_t)p4d,
             (uintptr_t)p4d_val(*p4d));
#if defined(p4d_leaf)
    if (p4d_leaf(*p4d)) {
        pr_debug("Address 0x%lx maps to a P4D-level huge page\n", addr);
        return KSU_P4D_TO_PHYS(*p4d) + ((addr & ~P4D_MASK));
    }
#elif defined(p4d_large)
    if (p4d_large(*p4d)) {
        return KSU_P4D_TO_PHYS(*p4d) + ((addr & ~P4D_MASK));
    }
#endif

    pud = pud_offset(p4d, addr);
    if (pud_none(*pud) || pud_bad(*pud))
        goto fail;
    pr_debug("pud of 0x%lx p=0x%lx v=0x%lx", addr, (uintptr_t)pud,
             (uintptr_t)pud_val(*pud));
#if defined(pud_leaf)
    if (pud_leaf(*pud)) {
        pr_debug("Address 0x%lx maps to a PUD-level huge page\n", addr);
        return KSU_PUD_TO_PHYS(*pud) + ((addr & ~PUD_MASK));
    }
#elif defined(pud_large)
    if (pud_large(*pud)) {
        return KSU_PUD_TO_PHYS(*pud) + ((addr & ~PUD_MASK));
    }
#endif

    pmd = pmd_offset(pud, addr);
    pr_debug("pmd of 0x%lx p=0x%lx v=0x%lx", addr, (uintptr_t)pmd,
             (uintptr_t)pmd_val(*pmd));
#if defined(pmd_leaf)
    if (pmd_leaf(*pmd)) {
        pr_debug("Address 0x%lx maps to a PMD-level huge page\n", addr);
        return KSU_PMD_TO_PHYS(*pmd) + ((addr & ~PMD_MASK));
    }
#elif defined(pmd_large)
    if (pmd_large(*pmd)) {
        return KSU_PMD_TO_PHYS(*pmd) + ((addr & ~PMD_MASK));
    }
#endif

    if (pmd_none(*pmd) || pmd_bad(*pmd))
        goto fail;

    pte = pte_offset_kernel(pmd, addr);
    if (!pte)
        goto fail;
    if (!pte_present(*pte))
        goto fail;

    return KSU_PTE_TO_PHYS(*pte) + ((addr & ~PAGE_MASK));

fail:
    *err = -ENOENT;
    return 0;
}

// --- Architecture-specific Cache Flushing & Barriers ---
#define ksu_flush_dcache(start, sz) do {} while (0)
#define ksu_flush_icache(start, end) do {} while (0)
#define ksu_isb() smp_mb()

struct patch_text_info {
    void *dst;
    void *src;
    size_t len;
    atomic_t cpu_count;
    int flags;
};

static int ksu_patch_text_nosync(void *dst, void *src, size_t len, int flags)
{
    pr_debug("patch dst=0x%lx src=0x%lx len=%ld\n", (unsigned long)dst,
             (unsigned long)src, len);

    unsigned long p = (unsigned long)dst;
    int ret;

    int phy_err;
    unsigned long phy = phys_from_virt(p, &phy_err);
    if (phy_err) {
        ret = phy_err;
        pr_err("failed to find phy addr for patch dst addr 0x%lx\n", p);
        goto err;
    }
    pr_debug("phy addr for patch 0x%lx: 0x%lx\n", p, phy);

    // Modern x86 removes FIX_TEXT_POKE0. FIX_BTMAP_BEGIN is universal and safe here.
    set_fixmap(FIX_BTMAP_BEGIN, phy & PAGE_MASK);
    void *map = (void *)(fix_to_virt(FIX_BTMAP_BEGIN) + (phy & ~PAGE_MASK));

    pr_debug("fixmap addr for patch 0x%lx: 0x%lx\n", p, (unsigned long)map);

    ret = (int)copy_to_kernel_nofault(map, src, len);

    clear_fixmap(FIX_BTMAP_BEGIN);

    if (!ret) {
        if (flags & KSU_PATCH_TEXT_FLUSH_ICACHE)
            ksu_flush_icache((uintptr_t)dst, (uintptr_t)dst + len);
        if (flags & KSU_PATCH_TEXT_FLUSH_DCACHE)
            ksu_flush_dcache(dst, len);
    }

err:
    pr_debug("patch result=%d\n", ret);
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
        ksu_isb();
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

#endif /* __x86_64__ */
