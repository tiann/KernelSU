#include <linux/mm.h>
#include <linux/pgtable.h>
#include <linux/printk.h>
#include <asm/current.h>

#include "util.h"

bool try_set_access_flag(unsigned long addr)
{
#ifdef CONFIG_ARM64
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vma;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep, pte;
    spinlock_t *ptl;
    bool ret = false;

    if (!mm)
        return false;

    if (!mmap_read_trylock(mm))
        return false;

    vma = find_vma(mm, addr);
    if (!vma || addr < vma->vm_start)
        goto out_unlock;

    pgd = pgd_offset(mm, addr);
    if (!pgd_present(*pgd))
        goto out_unlock;

    p4d = p4d_offset(pgd, addr);
    if (!p4d_present(*p4d))
        goto out_unlock;

    pud = pud_offset(p4d, addr);
    if (!pud_present(*pud))
        goto out_unlock;

    pmd = pmd_offset(pud, addr);
    if (!pmd_present(*pmd))
        goto out_unlock;

    if (pmd_trans_huge(*pmd))
        goto out_unlock;

    ptep = pte_offset_map_lock(mm, pmd, addr, &ptl);
    if (!ptep)
        goto out_unlock;

    pte = *ptep;

    if (!pte_present(pte))
        goto out_pte_unlock;

    if (pte_young(pte)) {
        ret = true;
        goto out_pte_unlock;
    }

    ptep_set_access_flags(vma, addr, ptep, pte_mkyoung(pte), 0);
    pr_info("set AF for addr %lx\n", addr);
    ret = true;

out_pte_unlock:
    pte_unmap_unlock(ptep, ptl);
out_unlock:
    mmap_read_unlock(mm);
    return ret;
#else
    return false;
#endif
}
