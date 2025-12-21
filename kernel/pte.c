#include "klog.h" // IWYU pragma: keep
#include "pte.h"

// https://github.com/fuqiuluo/ovo/blob/f7da411458e87d32438dc14fce5a3313ed0c967e/ovo/mmuhack.c#L21

unsigned long phys_from_virt(unsigned long addr)
{
    struct mm_struct *mm = &init_mm;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    pgd = pgd_offset(mm, addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        return NULL;
    pr_info("pgd of 0x%lx p=0x%lx v=0x%lx", addr, (uintptr_t)pgd,
            (uintptr_t)pgd_val(*pgd));

    p4d = p4d_offset(pgd, addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
        return NULL;
    pr_info("p4d of 0x%lx p=0x%lx v=0x%lx", addr, (uintptr_t)p4d,
            (uintptr_t)p4d_val(*p4d));
#if defined(p4d_leaf)
    if (p4d_leaf(*p4d)) {
        pr_info("Address 0x%lx maps to a P4D-level huge page\n", addr);
        return __p4d_to_phys(*p4d) + ((addr & ~P4D_MASK));
    }
#endif

    pud = pud_offset(p4d, addr);
    if (pud_none(*pud) || pud_bad(*pud))
        return NULL;
    pr_info("pud of 0x%lx p=0x%lx v=0x%lx", addr, (uintptr_t)pud,
            (uintptr_t)pud_val(*pud));
#if defined(pud_leaf)
    if (pud_leaf(*pud)) {
        pr_info("Address 0x%lx maps to a PUD-level huge page\n", addr);
        return __pud_to_phys(*pud) + ((addr & ~PUD_MASK));
    }
#endif

    pmd = pmd_offset(pud, addr);
    pr_info("pmd of 0x%lx p=0x%lx v=0x%lx", addr, (uintptr_t)pmd,
            (uintptr_t)pmd_val(*pmd));
#if defined(pmd_leaf)
    if (pmd_leaf(*pmd)) {
        pr_info("Address 0x%lx maps to a PMD-level huge page\n", addr);
        return __pmd_to_phys(*pmd) + ((addr & ~PMD_MASK));
    }
#endif

    if (pmd_none(*pmd) || pmd_bad(*pmd))
        return 0;

    pte = pte_offset_kernel(pmd, addr);
    if (!pte)
        return 0;
    if (!pte_present(*pte))
        return 0;

    return __pte_to_phys(*pte) + ((addr & ~PAGE_MASK));
}
