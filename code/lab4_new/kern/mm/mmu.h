#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#ifndef __ASSEMBLER__
#include <defs.h>
#endif /* !__ASSEMBLER__ */
//在这里sv39的页表项里从高到低三级页表的页码分别称作PDX1, PDX0和PTX

// The PDX1, PDX0, PTX, PGOFF, and PPN macros decompose linear addresses as shown.
// To construct a linear address la from PDX(la), PTX(la), and PGOFF(la),
// use PGADDR(PDX(la), PTX(la), PGOFF(la)).

// RISC-V uses 39-bit virtual address to access 56-bit physical address!
//合法 Sv39 地址要求第 63～39 位全部等于第 38 位，即高位必须满足符号扩展规则
// Sv39 virtual address:
// +----9----+----9---+----9---+---12--+
// |  VPN[2] | VPN[1] | VPN[0] | PGOFF |
// +---------+----+---+--------+-------+
//
// Sv39 physical address:
// +----26---+----9---+----9---+---12--+
// |  PPN[2] | PPN[1] | PPN[0] | PGOFF |
// +---------+----+---+--------+-------+
//
// Sv39 page table entry:
// +----26---+----9---+----9---+---2----+-------8-------+
// |  PPN[2] | PPN[1] | PPN[0] |RSW|D|A|G|U|X|W|R|V|
// +---------+----+---+--------+--------+---------------+


// page directory index
#define PDX1(la) ((((uintptr_t)(la)) >> PDX1SHIFT) & 0x1FF) //从地址 la 提取第 38～30 位。
#define PDX0(la) ((((uintptr_t)(la)) >> PDX0SHIFT) & 0x1FF)

// page table index
#define PTX(la) ((((uintptr_t)(la)) >> PTXSHIFT) & 0x1FF)//先去掉最低 12 位页内偏移，再取最低 9 位

// page number field of address
#define PPN(la) (((uintptr_t)(la)) >> PTXSHIFT)

// offset in page
#define PGOFF(la) (((uintptr_t)(la)) & 0xFFF)

// construct linear address from indexes and offset
#define PGADDR(d1, d0, t, o) ((uintptr_t)((d1) << PDX1SHIFT |(d0) << PDX0SHIFT | (t) << PTXSHIFT | (o)))
//只组合低 39 位，不会自动为高半区地址进行符号扩展

// address in page table or page directory entry
#define PTE_ADDR(pte)   (((uintptr_t)(pte) & ~0x3FF) << (PTXSHIFT - PTE_PPN_SHIFT))
//从当前实验格式的页表项中去掉低位属性，并把物理页号还原为物理页基址，本实验中表示取出 PPN，再将 PPN 左移 12 位

#define PDE_ADDR(pde)   PTE_ADDR(pde)
//对于非叶子表项，提取结果是下一级页表的物理基址；
// 对于叶子表项，提取结果是被映射物理区域的基址

/* page directory and page table constants */
#define NPDEENTRY       512                    // 上级页表page directory entries per page directory
#define NPTEENTRY       512                    // 最低级页表page table entries per page table

#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)
#define PTSIZE          (PGSIZE * NPTEENTRY)    // bytes mapped by a page directory entry
#define PTSHIFT         21                      // log2(PTSIZE)

#define PTXSHIFT        12                      // offset of PTX in a linear address
#define PDX0SHIFT       21                      // offset of PDX0 in a linear address
#define PDX1SHIFT		30                      // offset of PDX1 in a linear address
#define PTE_PPN_SHIFT   10                      // 表示物理页号在 PTE 中从第 10 位开始offset of PPN in a physical address

// page table entry (PTE) fields
#define PTE_V     0x001 // Valid
#define PTE_R     0x002 // Read
#define PTE_W     0x004 // Write
#define PTE_X     0x008 // Execute
#define PTE_U     0x010 // User
#define PTE_G     0x020 // Global
#define PTE_A     0x040 // Accessed
#define PTE_D     0x080 // Dirty
#define PTE_SOFT  0x300 // Reserved for Software

#define PAGE_TABLE_DIR (PTE_V)
#define READ_ONLY (PTE_R | PTE_V)
#define READ_WRITE (PTE_R | PTE_W | PTE_V)
#define EXEC_ONLY (PTE_X | PTE_V)
#define READ_EXEC (PTE_R | PTE_X | PTE_V)
#define READ_WRITE_EXEC (PTE_R | PTE_W | PTE_X | PTE_V)

#define PTE_USER (PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)

#endif /* !__KERN_MM_MMU_H__ */
