#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &best_fit_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 YOUR CODE
static void page_init(void) {
    uintptr_t mem_base = get_memory_base();
    uintptr_t mem_size = get_memory_size();
    const uintptr_t addr_max = (uintptr_t)-1;
    extern char end[];

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    if (mem_base == 0 || mem_size == 0 || mem_size > addr_max - mem_base) {
        panic("invalid physical memory information from DTB");
    }
    uintptr_t mem_end = mem_base + mem_size;
    uintptr_t kernel_end = PADDR(end);
    if (mem_base > KERNEL_BEGIN_PADDR || mem_end <= kernel_end ||
        kernel_end < KERNEL_BEGIN_PADDR) {
        panic("DTB memory does not contain the kernel");
    }
    cprintf("physical memory: [0x%016lx, 0x%016lx)\n", mem_base, mem_end);

    uintptr_t managed_end = mem_end;
    if (managed_end > PHYSICAL_MEMORY_END) {
        managed_end = PHYSICAL_MEMORY_END;
    }
    managed_end = ROUNDDOWN(managed_end, PGSIZE);
    if (managed_end <= DRAM_BASE || kernel_end > addr_max - (PGSIZE - 1)) {
        panic("no valid physical memory management range");
    }
    npage = PPN(managed_end);
    size_t count = npage - nbase;
    if (count > addr_max / sizeof(struct Page)) {
        panic("page descriptor size overflow");
    }

    // Place the descriptors after the kernel; validate before touching memory.
    uintptr_t descriptor_start = ROUNDUP(kernel_end, PGSIZE);
    size_t descriptor_size = count * sizeof(struct Page);
    if (descriptor_start < mem_base || descriptor_start > managed_end ||
        descriptor_size > managed_end - descriptor_start) {
        panic("not enough RAM for page descriptors");
    }
    uintptr_t descriptor_end = descriptor_start + descriptor_size;
    pages = (struct Page *)(descriptor_start + va_pa_offset);
    memset(pages, 0, descriptor_size);
    for (size_t i = 0; i < count; i ++) {
        SetPageReserved(pages + i);
        list_init(&(pages[i].page_link));
    }

    // Only complete pages beyond the kernel and descriptors may be allocated.
    uintptr_t free_start = ROUNDUP(descriptor_end, PGSIZE);
    if (free_start < managed_end) {
        init_memmap(pa2page(free_start), (managed_end - free_start) / PGSIZE);
    }
    cprintf("managed physical memory ends at 0x%016lx, free pages: %lu\n",
            managed_end, nr_free_pages());
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
