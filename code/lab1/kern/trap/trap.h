#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include <defs.h>

/* *
 * pushregs - the 32 general purpose registers, saved on the trap stack in
 * register-number order (x0 .. x31).
 * */
struct pushregs {
    uintptr_t zero;     // x0
    uintptr_t ra;       // x1
    uintptr_t sp;       // x2
    uintptr_t gp;       // x3
    uintptr_t tp;       // x4
    uintptr_t t0;       // x5
    uintptr_t t1;       // x6
    uintptr_t t2;       // x7
    uintptr_t s0;       // x8
    uintptr_t s1;       // x9
    uintptr_t a0;       // x10
    uintptr_t a1;       // x11
    uintptr_t a2;       // x12
    uintptr_t a3;       // x13
    uintptr_t a4;       // x14
    uintptr_t a5;       // x15
    uintptr_t a6;       // x16
    uintptr_t a7;       // x17
    uintptr_t s2;       // x18
    uintptr_t s3;       // x19
    uintptr_t s4;       // x20
    uintptr_t s5;       // x21
    uintptr_t s6;       // x22
    uintptr_t s7;       // x23
    uintptr_t s8;       // x24
    uintptr_t s9;       // x25
    uintptr_t s10;      // x26
    uintptr_t s11;      // x27
    uintptr_t t3;       // x28
    uintptr_t t4;       // x29
    uintptr_t t5;       // x30
    uintptr_t t6;       // x31
};

/* *
 * trapframe - the full context pushed by trapentry.S on every trap.
 * */
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;       // sstatus
    uintptr_t epc;          // sepc
    uintptr_t badvaddr;     // sbadaddr / stval
    uintptr_t cause;        // scause
};

void trap(struct trapframe *tf);
void trap_init(void);
void trap_dispatch(struct trapframe *tf);
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
void print_regs(struct pushregs *gpr);
void print_trapframe(struct trapframe *tf);

void clock_init(void);
void clock_set_next_event(void);
void intr_enable(void);
void intr_disable(void);

#endif /* !__KERN_TRAP_TRAP_H__ */
