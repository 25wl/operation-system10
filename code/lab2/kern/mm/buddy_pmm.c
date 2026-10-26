#include <buddy_pmm.h>
#include <list.h>

/* The machine exposes at most 128 MiB of RAM, or 32768 pages. */
#define BUDDY_MAX_ORDER 15

static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t free_count;

static size_t
block_size(unsigned int order) {
    return (size_t)1 << order;
}

static unsigned int
order_for(size_t n) {
    unsigned int order = 0;
    size_t size = 1;
    while (size < n && order < BUDDY_MAX_ORDER) {
        size <<= 1;
        order ++;
    }
    return order;
}

static void
add_block(struct Page *base, unsigned int order) {
    assert(!PageReserved(base) && !PageProperty(base));
    base->property = block_size(order);
    SetPageProperty(base);
    list_add(&free_lists[order], &base->page_link);
}

static void
remove_block(struct Page *base) {
    assert(PageProperty(base));
    list_del_init(&base->page_link);
    ClearPageProperty(base);
    base->property = 0;
}

/* Physical page numbers, rather than Page array indices, define buddies. */
static void
insert_and_merge(struct Page *base, unsigned int order) {
    while (order < BUDDY_MAX_ORDER) {
        size_t size = block_size(order);
        ppn_t buddy_ppn = page2ppn(base) ^ size;
        if (buddy_ppn < nbase || buddy_ppn >= npage ||
            size > npage - buddy_ppn) {
            break;
        }
        struct Page *buddy = &pages[buddy_ppn - nbase];
        if (PageReserved(buddy) || !PageProperty(buddy) ||
            buddy->property != size) {
            break;
        }
        remove_block(buddy);
        if (buddy < base) {
            base = buddy;
        }
        order ++;
    }
    add_block(base, order);
}

static void
buddy_init(void) {
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order ++) {
        list_init(&free_lists[order]);
    }
    free_count = 0;
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0 && base >= pages && n <= npage - page2ppn(base));
    for (size_t i = 0; i < n; i ++) {
        struct Page *page = base + i;
        assert(PageReserved(page));
        page->flags = 0;
        page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
    }

    /* Each block is aligned to its own size in physical page numbers. */
    for (size_t offset = 0; offset < n;) {
        unsigned int order = BUDDY_MAX_ORDER;
        ppn_t ppn = page2ppn(base + offset);
        while (block_size(order) > n - offset ||
               (ppn & (block_size(order) - 1)) != 0) {
            order --;
        }
        insert_and_merge(base + offset, order);
        offset += block_size(order);
    }
    free_count += n;
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > block_size(BUDDY_MAX_ORDER)) {
        return NULL;
    }
    unsigned int wanted = order_for(n), order = wanted;
    while (order <= BUDDY_MAX_ORDER && list_empty(&free_lists[order])) {
        order ++;
    }
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }

    struct Page *base = le2page(list_next(&free_lists[order]), page_link);
    remove_block(base);
    while (order > wanted) {
        order --;
        add_block(base + block_size(order), order);
    }
    /* A non-power-of-two request owns the complete rounded-up block. */
    base->property = block_size(wanted);
    free_count -= base->property;
    return base;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && base >= pages && page2ppn(base) < npage);
    assert(n <= block_size(BUDDY_MAX_ORDER));
    unsigned int order = order_for(n);
    size_t size = block_size(order);
    /* Reject a wrong size class, interior pointer, or duplicate free. */
    assert(!PageReserved(base) && !PageProperty(base) &&
           base->property == size && (page2ppn(base) & (size - 1)) == 0 &&
           size <= npage - page2ppn(base));
    for (size_t i = 0; i < size; i ++) {
        struct Page *page = base + i;
        assert(!PageReserved(page) && !PageProperty(page));
        page->flags = 0;
        page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
    }
    insert_and_merge(base, order);
    free_count += size;
}

static size_t
buddy_nr_free_pages(void) {
    return free_count;
}

static void
check_free_blocks(size_t expected) {
    size_t total = 0;
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order ++) {
        for (list_entry_t *le = list_next(&free_lists[order]);
             le != &free_lists[order]; le = list_next(le)) {
            struct Page *page = le2page(le, page_link);
            assert(!PageReserved(page) && PageProperty(page));
            assert(page->property == block_size(order));
            assert((page2ppn(page) & (block_size(order) - 1)) == 0);
            total += page->property;
        }
    }
    assert(total == expected && free_count == expected);
}

static void
buddy_test_scenarios(void) {
    size_t before = free_count;
    check_free_blocks(before);

    /* Hold the other half of a 32-page allocation so the test is isolated. */
    struct Page *arena = buddy_alloc_pages(32);
    assert(arena != NULL);
    assert(free_count == before - 32 && arena->property == 32 &&
           !PageProperty(arena) && (page2ppn(arena) & 31) == 0);
    list_entry_t saved[BUDDY_MAX_ORDER + 1];
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order ++) {
        saved[order] = free_lists[order];
        list_init(&free_lists[order]);
    }
    size_t saved_count = free_count;
    free_count = 0;

    /* An unaligned five-page range decomposes as 1 + 2 + 2. */
    for (size_t i = 1; i < 6; i ++) {
        SetPageReserved(arena + i);
    }
    buddy_init_memmap(arena + 1, 5);
    check_free_blocks(5);
    assert(PageProperty(arena + 1) && arena[1].property == 1);
    assert(PageProperty(arena + 2) && arena[2].property == 2);
    assert(PageProperty(arena + 4) && arena[4].property == 2);
    assert(!PageProperty(arena) && !PageProperty(arena + 6));

    struct Page *one = buddy_alloc_pages(1);
    struct Page *left = buddy_alloc_pages(2);
    struct Page *right = buddy_alloc_pages(2);
    assert(one == arena + 1 && left == arena + 4 && right == arena + 2);
    assert(free_count == 0 && buddy_alloc_pages(1) == NULL);
    check_free_blocks(0);

    buddy_free_pages(left, 2);
    buddy_free_pages(right, 2);
    check_free_blocks(4);
    /* These size-two blocks touch, but their physical XOR buddies differ. */
    assert(PageProperty(arena + 2) && arena[2].property == 2);
    assert(PageProperty(arena + 4) && arena[4].property == 2);
    list_entry_t *head = list_next(&free_lists[1]);
    list_entry_t *tail = list_prev(&free_lists[1]);
    assert(buddy_alloc_pages(4) == NULL);
    assert(list_next(&free_lists[1]) == head &&
           list_prev(&free_lists[1]) == tail);
    check_free_blocks(4);
    assert(PageProperty(arena + 2) && PageProperty(arena + 4));

    buddy_free_pages(one, 1);
    check_free_blocks(5);
    assert(buddy_alloc_pages(2) == arena + 2);
    assert(buddy_alloc_pages(2) == arena + 4);
    assert(buddy_alloc_pages(1) == arena + 1);
    check_free_blocks(0);

    /* Rebuild the isolated half as one aligned 16-page test range. */
    for (size_t i = 0; i < 16; i ++) {
        arena[i].flags = 0;
        SetPageReserved(arena + i);
        arena[i].property = 0;
    }
    buddy_init_memmap(arena, 3);
    check_free_blocks(3);
    buddy_init_memmap(arena + 3, 13);
    check_free_blocks(16);
    assert(PageProperty(arena) && arena->property == 16);

    struct Page *a = buddy_alloc_pages(3);
    struct Page *b = buddy_alloc_pages(5);
    struct Page *c = buddy_alloc_pages(4);
    assert(a == arena && b == arena + 8 && c == arena + 4);
    assert(a->property == 4 && b->property == 8 && c->property == 4);
    assert(!PageProperty(a) && !PageProperty(b) && !PageProperty(c));
    check_free_blocks(0);
    assert(buddy_alloc_pages(1) == NULL);
    check_free_blocks(0);
    buddy_free_pages(c, 4);
    check_free_blocks(4);
    buddy_free_pages(a, 3);
    check_free_blocks(8);
    assert(PageProperty(arena) && arena->property == 8);
    a = buddy_alloc_pages(8);
    assert(a == arena);
    check_free_blocks(0);
    buddy_free_pages(b, 5);
    buddy_free_pages(a, 8);
    check_free_blocks(16);
    assert(PageProperty(arena) && arena->property == 16);
    assert(buddy_alloc_pages(16) == arena);
    check_free_blocks(0);

    arena->property = 32;
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order ++) {
        assert(list_empty(&free_lists[order]));
        free_lists[order] = saved[order];
    }
    free_count = saved_count;
    buddy_free_pages(arena, 32);
    check_free_blocks(before);
}

static void
buddy_check(void) {
    buddy_test_scenarios();
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
