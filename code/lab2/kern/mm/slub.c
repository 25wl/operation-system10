#include <slub.h>
#include <pmm.h>
#include <list.h>

struct slub_cache {
    size_t object_size;
    list_entry_t partial;
};

/* The header lives at the start of the original PMM allocation. */
struct slub_header {
    struct Page *base;
    size_t page_count;
    struct slub_cache *cache; /* NULL identifies a large allocation. */
    size_t capacity;
    size_t inuse;
    void *free_objects;
    list_entry_t link;
};

static struct slub_cache caches[] = {
    {.object_size = 16}, {.object_size = 32},
    {.object_size = 64}, {.object_size = 128},
    {.object_size = 256}, {.object_size = 512},
    {.object_size = 1024}, {.object_size = 2048},
};
static bool initialized;

#define HEADER_SIZE ROUNDUP(sizeof(struct slub_header), SLUB_ALIGNMENT)
#define CACHE_COUNT (sizeof(caches) / sizeof(caches[0]))

static struct slub_header *
slub_new_pages(size_t count) {
    if (count > nr_free_pages()) {
        return NULL;
    }
    struct Page *base = alloc_pages(count);
    if (base == NULL) {
        return NULL;
    }
    struct slub_header *header =
        (struct slub_header *)(page2pa(base) + va_pa_offset);
    header->base = base;
    header->page_count = count;
    header->cache = NULL;
    header->capacity = header->inuse = 0;
    header->free_objects = NULL;
    list_init(&header->link);
    return header;
}

void
slub_init(void) {
    assert(!initialized);
    assert(HEADER_SIZE < PGSIZE);
    assert(HEADER_SIZE % SLUB_ALIGNMENT == 0);
    for (size_t i = 0; i < CACHE_COUNT; i ++) {
        list_init(&caches[i].partial);
    }
    initialized = 1;
}

void *
slub_alloc(size_t size) {
    assert(initialized);
    if (size == 0) {
        return NULL;
    }
    if (size > SLUB_SMALL_MAX) {
        /* Check before addition; division/remainder avoids rounding overflow. */
        if (size > (size_t)-1 - HEADER_SIZE) {
            return NULL;
        }
        size_t bytes = size + HEADER_SIZE;
        size_t count = bytes / PGSIZE + (bytes % PGSIZE != 0);
        if (npage <= nbase || count > npage - nbase) {
            return NULL;
        }
        struct slub_header *header = slub_new_pages(count);
        return header == NULL ? NULL : (char *)header + HEADER_SIZE;
    }

    size_t index = 0;
    while (caches[index].object_size < size) {
        index ++;
    }
    struct slub_cache *cache = &caches[index];
    struct slub_header *header;
    if (list_empty(&cache->partial)) {
        header = slub_new_pages(1);
        if (header == NULL) {
            return NULL;
        }
        header->cache = cache;
        header->capacity = (PGSIZE - HEADER_SIZE) / cache->object_size;
        /* Free-list links occupy slots only while those slots are free. */
        for (size_t i = 0; i < header->capacity; i ++) {
            void *slot = (char *)header + HEADER_SIZE + i * cache->object_size;
            *(void **)slot = header->free_objects;
            header->free_objects = slot;
        }
        list_add(&cache->partial, &header->link);
    } else {
        header = to_struct(list_next(&cache->partial), struct slub_header, link);
    }
    void *object = header->free_objects;
    header->free_objects = *(void **)object;
    header->inuse ++;
    if (header->inuse == header->capacity) {
        list_del_init(&header->link);
    }
    return object;
}

void
slub_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    assert(initialized);
    /* Validate the direct-map range before reading a page header. */
    uintptr_t address = (uintptr_t)ptr;
    assert(address >= KERNBASE && address >= va_pa_offset);
    uintptr_t pa = address - va_pa_offset;
    assert(pa / PGSIZE >= nbase && pa / PGSIZE < npage);
    assert(address % SLUB_ALIGNMENT == 0);
    struct slub_header *header =
        (struct slub_header *)(address - pa % PGSIZE);
    struct Page *base = header->base;
    size_t count = header->page_count;
    if (header->cache == NULL) {
        assert(ptr == (void *)((char *)header + HEADER_SIZE));
        free_pages(base, count);
        return;
    }

    struct slub_cache *cache = header->cache;
    size_t offset = address - (uintptr_t)header;
    assert(offset >= HEADER_SIZE);
    assert((offset - HEADER_SIZE) % cache->object_size == 0);
    assert((offset - HEADER_SIZE) / cache->object_size < header->capacity);
    assert(header->inuse > 0);
    bool was_full = header->inuse == header->capacity;
    header->inuse --;
    if (header->inuse == 0) {
        if (!was_full) {
            list_del(&header->link);
        }
        free_pages(base, count);
        return;
    }
    *(void **)ptr = header->free_objects;
    header->free_objects = ptr;
    if (was_full) {
        list_add(&cache->partial, &header->link);
    }
}
