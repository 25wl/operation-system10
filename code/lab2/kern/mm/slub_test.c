#include <slub_test.h>
#include <slub.h>
#include <pmm.h>
#include <stdio.h>

#define RECORD_COUNT (PGSIZE / 64 + 1)

struct test_record {
    unsigned char *ptr;
    size_t size;
    unsigned int seed;
};

static struct test_record records[RECORD_COUNT];

static unsigned char
pattern(unsigned int seed, size_t offset) {
    return (unsigned char)(seed * 37 + offset * 13 + offset / 251);
}

static void
verify_record(size_t index) {
    struct test_record *r = &records[index];
    assert(r->ptr != NULL);
    for (size_t i = 0; i < r->size; i ++) {
        assert(r->ptr[i] == pattern(r->seed, i));
    }
}

static void
verify_live(void) {
    for (size_t i = 0; i < RECORD_COUNT; i ++) {
        if (records[i].ptr != NULL) {
            verify_record(i);
        }
    }
}

static void
allocate_record(size_t index, size_t size, unsigned int seed) {
    assert(index < RECORD_COUNT && records[index].ptr == NULL);
    unsigned char *ptr = slub_alloc(size);
    assert(ptr != NULL);
    uintptr_t start = (uintptr_t)ptr;
    assert(start % SLUB_ALIGNMENT == 0);
    assert(size <= (uintptr_t)-1 - start);
    uintptr_t end = start + size;
    /* Compare before writing, so overlap cannot silently corrupt a neighbour. */
    for (size_t i = 0; i < RECORD_COUNT; i ++) {
        if (records[i].ptr != NULL) {
            uintptr_t other = (uintptr_t)records[i].ptr;
            assert(records[i].size <= (uintptr_t)-1 - other);
            assert(end <= other || other + records[i].size <= start);
        }
    }
    records[index].ptr = ptr;
    records[index].size = size;
    records[index].seed = seed;
    for (size_t i = 0; i < size; i ++) {
        ptr[i] = pattern(seed, i);
    }
}

static void
release_record(size_t index) {
    verify_record(index);
    slub_free(records[index].ptr);
    records[index].ptr = NULL;
}

static void
release_all(void) {
    for (size_t i = 0; i < RECORD_COUNT; i ++) {
        if (records[i].ptr != NULL) {
            release_record(i);
        }
    }
}

static void
check_boundaries(size_t baseline) {
    static const size_t classes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    static const size_t edges[] = {1, PGSIZE - 1, PGSIZE, PGSIZE + 1,
                                 2 * PGSIZE + 37};
    for (size_t i = 0; i < sizeof(classes) / sizeof(classes[0]); i ++) {
        for (size_t delta = 0; delta < 3; delta ++) {
            allocate_record(0, classes[i] - 1 + delta, (unsigned int)i + 1);
            release_record(0);
            assert(nr_free_pages() == baseline);
        }
    }
    for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i ++) {
        allocate_record(0, edges[i], (unsigned int)i + 20);
        release_record(0);
        assert(nr_free_pages() == baseline);
    }
}

void
slub_check(void) {
    size_t baseline = nr_free_pages();
    if (baseline < 16) {
        panic("slub_check requires at least 16 free physical pages");
    }
    for (size_t i = 0; i < RECORD_COUNT; i ++) {
        assert(records[i].ptr == NULL);
    }

    cprintf("slub_check: case 1 zero size and NULL free\n");
    assert(slub_alloc(0) == NULL);
    assert(nr_free_pages() == baseline);
    slub_free(NULL);
    assert(nr_free_pages() == baseline);

    cprintf("slub_check: case 2 size boundaries\n");
    check_boundaries(baseline);

    cprintf("slub_check: case 3 mixed live objects\n");
    static const size_t mixed[] = {1, 17, 33, 65, 129};
    /* One three-page object plus 63 small ones keeps demand below 16 pages. */
    for (size_t i = 0; i < 64; i ++) {
        size_t size = i == 63 ? 2 * PGSIZE + 37 : mixed[i % 5];
        allocate_record(i, size, (unsigned int)i + 40);
        verify_live();
    }
    release_all();
    assert(nr_free_pages() == baseline);

    cprintf("slub_check: case 4 shared slab page\n");
    allocate_record(0, 16, 110);
    size_t after_first = nr_free_pages();
    allocate_record(1, 16, 111);
    assert(nr_free_pages() == after_first);
    verify_live();
    release_all();
    assert(nr_free_pages() == baseline);

    cprintf("slub_check: case 5 multiple slabs and partial reuse\n");
    for (size_t i = 0; i < RECORD_COUNT; i ++) {
        allocate_record(i, 64, (unsigned int)i + 120);
    }
    assert(nr_free_pages() <= baseline && baseline - nr_free_pages() >= 2);
    for (size_t i = 0; i < RECORD_COUNT - 2; i += 2) {
        release_record(i);
        verify_live();
    }
    size_t before_refill = nr_free_pages();
    /* The inspected implementation has 63 slots per 64-byte slab. Leaving
     * the last two objects alive keeps both slabs allocated; no private
     * fields or free-list order are consulted by the test. */
    for (size_t i = 0; i < RECORD_COUNT - 2; i += 2) {
        allocate_record(i, 64, (unsigned int)i + 190);
        assert(nr_free_pages() == before_refill);
        verify_live();
    }
    release_all();
    assert(nr_free_pages() == baseline);

    cprintf("slub_check: case 6 release orders, 10 rounds each\n");
    for (unsigned int order = 0; order < 3; order ++) {
        for (unsigned int round = 0; round < 10; round ++) {
            for (size_t i = 0; i < 32; i ++) {
                allocate_record(i, 64, (unsigned int)i + round);
            }
            for (size_t i = 0; i < 32; i ++) {
                size_t index = order == 0 ? i : order == 1 ? 31 - i :
                    (i < 16 ? 2 * i : 2 * (i - 16) + 1);
                release_record(index);
                verify_live();
            }
            assert(nr_free_pages() == baseline);
        }
    }

    cprintf("slub_check: case 7 large allocation reclamation\n");
    allocate_record(0, 31, 10);
    size_t with_small = nr_free_pages();
    allocate_record(1, 2 * PGSIZE + 37, 11);
    allocate_record(2, 129, 12);
    verify_live();
    release_record(2);
    release_record(1);
    assert(nr_free_pages() == with_small);
    verify_record(0);
    release_record(0);
    assert(nr_free_pages() == baseline);

    cprintf("slub_check: case 8 overflow and allocation failure\n");
    allocate_record(0, 37, 80);
    size_t before_failure = nr_free_pages();
    assert(slub_alloc((size_t)-1) == NULL);
    assert(nr_free_pages() == before_failure);
    verify_record(0);
    allocate_record(1, 64, 81);
    release_record(1);
    assert(nr_free_pages() == before_failure);
    assert(before_failure <= ((size_t)-1 - 1) / PGSIZE);
    size_t impossible = before_failure * PGSIZE + 1;
    assert(slub_alloc(impossible) == NULL);
    assert(nr_free_pages() == before_failure);
    verify_record(0);
    allocate_record(1, 64, 82);
    verify_live();
    release_all();
    assert(nr_free_pages() == baseline);
    cprintf("slub_check() succeeded!\n");
}
