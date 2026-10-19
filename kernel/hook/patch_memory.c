/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/cpu.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/pgtable.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#include <linux/stop_machine.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <asm/tlbflush.h>

#include "patch_memory.h"

struct patch_mapping {
    unsigned long base;
    unsigned long size;
    void *entry;
    unsigned int count;
    unsigned int napot_order;
    bool pmd;
    bool ro;
};

struct ro_page {
    struct rb_node node;
    struct patch_mapping mapping;
    struct page *pages;
    void *original;
    bool installed;
    unsigned long saved[];
};

struct patch_range {
    struct list_head list;
    unsigned long addr;
    size_t len;
    struct ro_page *replacement;
};

/* Includes preparation, fixmap writes and reclamation, so another patch can
 * neither duplicate a replacement nor race with a page being restored/freed.
 */
static DEFINE_MUTEX(patch_memory_lock);
static struct rb_root ro_pages = RB_ROOT;

static bool mapping_is_ro(unsigned long value)
{
#ifdef CONFIG_ARM64
    /* PTE_WRITE is the DBM bit on hardware with dirty-bit management. A clean
     * writable mapping can also have PTE_RDONLY set; it must not be copied.
     */
    return (value & PTE_VALID) && (value & PTE_RDONLY) && !(value & PTE_WRITE) && (value & PTE_PXN) &&
           !(value & PTE_USER);
#elif defined(CONFIG_X86_64)
    return (value & _PAGE_PRESENT) && !(value & (_PAGE_RW | _PAGE_USER)) && (value & _PAGE_NX);
#else
    return (value & _PAGE_PRESENT) && (value & _PAGE_READ) && !(value & (_PAGE_WRITE | _PAGE_EXEC | _PAGE_USER));
#endif
}

static bool table_leaf(unsigned long value)
{
#ifdef CONFIG_ARM64
    return (value & PMD_TYPE_MASK) == PMD_TYPE_SECT;
#elif defined(CONFIG_X86_64)
    return value & _PAGE_PSE;
#else
    return value & _PAGE_LEAF;
#endif
}

static int describe_mapping(struct patch_mapping *map, unsigned long addr, void *entry, unsigned long value,
                            unsigned long size, bool pmd)
{
    *map = (struct patch_mapping){
        .base = addr & ~(size - 1),
        .size = size,
        .entry = entry,
        .count = 1,
        .pmd = pmd,
        .ro = mapping_is_ro(value),
    };
    if (!map->ro)
        return 0;

#ifdef CONFIG_ARM64
    if (value & PTE_CONT) {
        map->size = pmd ? CONT_PMD_SIZE : CONT_PTE_SIZE;
        map->count = map->size / size;
    }
#elif defined(CONFIG_RISCV_ISA_SVNAPOT)
    if (!pmd && pte_napot(__pte(value))) {
        map->napot_order = napot_cont_order(__pte(value));
        map->count = 1U << map->napot_order;
        map->size = PAGE_SIZE << map->napot_order;
    }
#endif
    if (map->size > SZ_2M)
        return -E2BIG;

    map->base = addr & ~(map->size - 1);
    if (pmd)
        map->entry = (pmd_t *)entry - (addr - map->base) / size;
    else
        map->entry = (pte_t *)entry - (addr - map->base) / size;
    return 0;
}

static int lookup_mapping(unsigned long addr, struct patch_mapping *map)
{
    pgd_t *pgd = pgd_offset_k(addr);
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    if (pgd_none(*pgd))
        return -ENOENT;
    if (table_leaf(pgd_val(*pgd)))
        return describe_mapping(map, addr, pgd, pgd_val(*pgd), PGDIR_SIZE, true);
    if (pgd_bad(*pgd))
        return -ENOENT;
    p4d = p4d_offset(pgd, addr);
    if (p4d_none(*p4d))
        return -ENOENT;
    if (table_leaf(p4d_val(*p4d)))
        return describe_mapping(map, addr, p4d, p4d_val(*p4d), P4D_SIZE, true);
    if (p4d_bad(*p4d))
        return -ENOENT;
    pud = pud_offset(p4d, addr);
    if (pud_none(*pud))
        return -ENOENT;
    if (table_leaf(pud_val(*pud)))
        return describe_mapping(map, addr, pud, pud_val(*pud), PUD_SIZE, true);
    if (pud_bad(*pud))
        return -ENOENT;
    pmd = pmd_offset(pud, addr);
    if (pmd_none(*pmd))
        return -ENOENT;
    if (table_leaf(pmd_val(*pmd)))
        return describe_mapping(map, addr, pmd, pmd_val(*pmd), PMD_SIZE, true);
    if (pmd_bad(*pmd))
        return -ENOENT;
    pte = pte_offset_kernel(pmd, addr);
    if (!pte || !pte_present(*pte))
        return -ENOENT;
    return describe_mapping(map, addr, pte, pte_val(*pte), PAGE_SIZE, false);
}

static unsigned long read_entry(const struct patch_mapping *map, unsigned int i)
{
    if (map->pmd)
        return pmd_val(READ_ONCE(((pmd_t *)map->entry)[i]));
    return pte_val(READ_ONCE(((pte_t *)map->entry)[i]));
}

static void write_entry(const struct patch_mapping *map, unsigned int i, unsigned long value)
{
    if (map->pmd) {
        set_pmd((pmd_t *)map->entry + i, __pmd(value));
        return;
    }
    /* ARM64's newer set_pte() strips PTE_CONT. We update the whole contiguous
     * group, including the break-before-make sequence, ourselves.
     */
#if defined(CONFIG_ARM64) && defined(CONFIG_ARM64_CONTPTE)
    __set_pte((pte_t *)map->entry + i, __pte(value));
#else
    set_pte((pte_t *)map->entry + i, __pte(value));
#endif
}

static unsigned long entry_pfn(const struct patch_mapping *map, unsigned long value)
{
    if (map->pmd)
        return pmd_pfn(__pmd(value));
    return pte_pfn(__pte(value));
}

static unsigned long replacement_entry(const struct ro_page *page, unsigned int i)
{
    const struct patch_mapping *map = &page->mapping;
    unsigned long value = page->saved[i];
    unsigned long pfn = page_to_pfn(page->pages) + i * (map->pmd ? PMD_SIZE : PAGE_SIZE) / PAGE_SIZE;

#ifdef CONFIG_ARM64
    /* Use the architecture's address encoding, including 52-bit PA layouts,
     * to remove the old PFN without discarding descriptor attributes.
     */
    return (value ^ __phys_to_pte_val(__pte_to_phys(__pte(value)))) | __phys_to_pte_val(PFN_PHYS(pfn));
#elif defined(CONFIG_X86_64)
    unsigned long mask = map->pmd ? pmd_pfn_mask(__pmd(value)) : PTE_PFN_MASK;

    return (value & ~mask) | (PFN_PHYS(pfn) & mask);
#else
    if (map->napot_order)
        pfn = page_to_pfn(page->pages) | BIT(map->napot_order - 1);
    return (value & ~_PAGE_PFN_MASK) | (pfn << _PAGE_PFN_SHIFT);
#endif
}

static bool entry_matches(unsigned long a, unsigned long b)
{
    /* Hardware may set access/dirty bits between preparation and stop_machine. */
#ifdef CONFIG_ARM64
    unsigned long volatile_bits = PTE_AF;
#else
    unsigned long volatile_bits = _PAGE_ACCESSED | _PAGE_DIRTY;
#endif

    return !((a ^ b) & ~volatile_bits);
}

static struct ro_page *find_ro_page(unsigned long addr)
{
    struct rb_node *node = ro_pages.rb_node;

    while (node) {
        struct ro_page *page = rb_entry(node, struct ro_page, node);

        if (addr < page->mapping.base)
            node = node->rb_left;
        else if (addr - page->mapping.base >= page->mapping.size)
            node = node->rb_right;
        else
            return page;
    }
    return NULL;
}

static void insert_ro_page(struct ro_page *page)
{
    struct rb_node **node = &ro_pages.rb_node, *parent = NULL;

    while (*node) {
        struct ro_page *other = rb_entry(*node, struct ro_page, node);

        parent = *node;
        node = page->mapping.base < other->mapping.base ? &parent->rb_left : &parent->rb_right;
    }
    rb_link_node(&page->node, parent, node);
    rb_insert_color(&page->node, &ro_pages);
}

static struct ro_page *alloc_ro_page(const struct patch_mapping *map)
{
    struct ro_page *page;
    unsigned long first_pfn;
    unsigned int i;
    int err = -EINVAL;

    page = kzalloc(struct_size(page, saved, map->count), GFP_KERNEL);
    if (!page)
        return ERR_PTR(-ENOMEM);
    page->mapping = *map;
    for (i = 0; i < map->count; i++)
        page->saved[i] = read_entry(map, i);
    first_pfn = entry_pfn(map, page->saved[0]);
    for (i = 0; i < map->count; i++) {
        unsigned long expected = first_pfn + i * (map->pmd ? PMD_SIZE : PAGE_SIZE) / PAGE_SIZE;

        if (map->napot_order)
            expected = first_pfn;
        if (!mapping_is_ro(page->saved[i]) || entry_pfn(map, page->saved[i]) != expected)
            goto fail;
    }
    /* Only RAM can be replaced with allocated pages, never MMIO mappings. */
    for (i = 0; i < map->size / PAGE_SIZE; i++) {
        if (!pfn_valid(first_pfn + i))
            goto fail;
    }
    page->original = page_address(pfn_to_page(first_pfn));
    page->pages = alloc_pages(GFP_KERNEL | __GFP_NOWARN, get_order(map->size));
    if (!page->pages) {
        err = -ENOMEM;
        goto fail;
    }
    memcpy(page_address(page->pages), (void *)map->base, map->size);
    insert_ro_page(page);
    return page;
fail:
    kfree(page);
    return ERR_PTR(err);
}

struct remap_info {
    struct ro_page *page;
    bool restore;
    atomic_t arrived;
    int result;
};

static int remap_cpu(void *data)
{
    struct remap_info *info = data;
    struct ro_page *page = info->page;
    const struct patch_mapping *map = &page->mapping;
    unsigned int i;

    if (atomic_inc_return(&info->arrived) == num_online_cpus()) {
        for (i = 0; i < map->count; i++) {
            unsigned long expected = info->restore ? replacement_entry(page, i) : page->saved[i];

            if (!entry_matches(read_entry(map, i), expected)) {
                info->result = -EAGAIN;
                goto done;
            }
        }
#ifdef CONFIG_ARM64
        /* Changing an output address requires break-before-make, including
         * every descriptor covered by a contiguous hint. TLBI is a hardware
         * broadcast here and does not require IPIs to the stopped CPUs.
         */
        for (i = 0; i < map->count; i++)
            write_entry(map, i, 0);
        flush_tlb_kernel_range(map->base, map->base + map->size);
#endif
        for (i = 0; i < map->count; i++)
            write_entry(map, i, info->restore ? page->saved[i] : replacement_entry(page, i));
        page->installed = !info->restore;
    done:
        atomic_inc_return_release(&info->arrived);
    } else {
        while (atomic_read_acquire(&info->arrived) <= num_online_cpus())
            cpu_relax();
    }
    /* Never use an IPI-based TLB shootdown inside stop_machine. Each CPU
     * flushes locally and stop_machine waits for all of them before we free.
     */
#ifdef CONFIG_X86_64
    __flush_tlb_all();
#elif defined(CONFIG_RISCV)
    local_flush_tlb_all();
#else
    isb();
#endif
    return 0;
}

static int remap_page(struct ro_page *page, bool restore)
{
    struct remap_info info = {
        .page = page,
        .restore = restore,
        .arrived = ATOMIC_INIT(0),
    };
    int ret = stop_machine(remap_cpu, &info, cpu_online_mask);

    return ret ? ret : info.result;
}

static void free_ro_page(struct ro_page *page)
{
    rb_erase(&page->node, &ro_pages);
    __free_pages(page->pages, get_order(page->mapping.size));
    kfree(page);
}

/* Build the complete plan before changing any mapping or data. In particular,
 * a later unsupported block or allocation failure leaves earlier pages alone.
 */
static int prepare_ranges(struct list_head *ranges, unsigned long addr, size_t len)
{
    while (len) {
        struct patch_mapping map;
        struct ro_page *page = find_ro_page(addr);
        struct patch_range *range;
        int ret;

        ret = lookup_mapping(addr, &map);
        if (ret)
            return ret;
        if (page) {
            unsigned int i;

            if (map.entry != page->mapping.entry || map.base != page->mapping.base || map.size != page->mapping.size ||
                map.count != page->mapping.count || map.pmd != page->mapping.pmd || !map.ro)
                return -EAGAIN;
            for (i = 0; i < map.count; i++) {
                unsigned long expected = page->installed ? replacement_entry(page, i) : page->saved[i];

                if (!entry_matches(read_entry(&map, i), expected))
                    return -EAGAIN;
            }
        }
        range = kzalloc(sizeof(*range), GFP_KERNEL);
        if (!range)
            return -ENOMEM;
        if (!page && map.ro) {
            page = alloc_ro_page(&map);
            if (IS_ERR(page)) {
                kfree(range);
                return PTR_ERR(page);
            }
        }
        range->addr = addr;
        range->len = min_t(size_t, len, map.size - (addr - map.base));
        range->replacement = page;
        addr += range->len;
        len -= range->len;
        /* Preserve the direct writer's single stop_machine operation for a
         * contiguous RX/RW patch, even when it spans multiple PTEs.
         */
        if (!page && !list_empty(ranges)) {
            struct patch_range *last = list_last_entry(ranges, struct patch_range, list);

            if (!last->replacement) {
                last->len += range->len;
                kfree(range);
                continue;
            }
        }
        list_add_tail(&range->list, ranges);
    }
    return 0;
}

int ksu_patch_memory(void *dst, void *src, size_t len, int flags)
{
    LIST_HEAD(ranges);
    struct patch_range *range, *next;
    unsigned long end;
    void *source;
    int ret;

    if (!len)
        return 0;
    if (!dst || !src || (unsigned long)dst < TASK_SIZE || check_add_overflow((unsigned long)dst, len, &end))
        return -EINVAL;
    source = kvmalloc(len, GFP_KERNEL);
    if (!source)
        return -ENOMEM;
    mutex_lock(&patch_memory_lock);
    /* Snapshot the source before remapping, including overlapping patches. */
    ret = copy_from_kernel_nofault(source, src, len);
    if (ret)
        goto out_ranges;

    ret = prepare_ranges(&ranges, (unsigned long)dst, len);
    if (ret)
        goto out_ranges;
    list_for_each_entry (range, &ranges, list) {
        struct ro_page *page = range->replacement;

        if (page && !page->installed) {
            ret = remap_page(page, false);
            if (ret)
                goto out_ranges;
        }
    }
    list_for_each_entry (range, &ranges, list) {
        unsigned long addr = range->addr;
        size_t remaining = range->len;

        if (!range->replacement) {
            ret = ksu_patch_text((void *)addr, source + (addr - (unsigned long)dst), remaining, flags);
            if (ret)
                goto out_ranges;
            continue;
        }
        while (remaining) {
            size_t size = min_t(size_t, remaining, PAGE_SIZE - offset_in_page(addr));

            ret = ksu_patch_text((void *)addr, source + (addr - (unsigned long)dst), size, flags);
            if (ret)
                goto out_ranges;
            addr += size;
            remaining -= size;
        }
    }
out_ranges:
    list_for_each_entry_safe (range, next, &ranges, list) {
        struct ro_page *page = range->replacement;

        if (page) {
            /* Compare the entire mapping: restoring one hook must not discard
             * other patches sharing a page (or a 2 MiB block).
             */
            if (page->installed && !memcmp(page_address(page->pages), page->original, page->mapping.size)) {
                int err = remap_page(page, true);

                if (!ret)
                    ret = err;
            }
            if (!page->installed)
                free_ro_page(page);
        }
        list_del(&range->list);
        kfree(range);
    }
    mutex_unlock(&patch_memory_lock);
    kvfree(source);
    return ret;
}
