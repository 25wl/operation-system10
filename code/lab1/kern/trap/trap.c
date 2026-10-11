#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <sbi.h>
#include <trap.h>

/* the number of timer interrupts between two "100 ticks" messages */
#define TICK_NUM 100

/* the interval (in mtime ticks) between two timer interrupts.
 * QEMU's `virt` machine has a 10 MHz timebase, so 1000000 ticks ~= 0.1s. */
#define TIMER_INTERVAL 1000000

volatile size_t ticks;

/* print_ticks - print a message every TICK_NUM timer interrupts */
static void print_ticks(void) {
    cprintf("%d ticks\n", TICK_NUM);
}

/* clock_set_next_event - program the next timer interrupt */
void clock_set_next_event(void) {
    sbi_set_timer(rdtime() + TIMER_INTERVAL);
}

/* clock_init - enable the supervisor timer interrupt and set the first event */
void clock_init(void) {
    // enable the supervisor timer interrupt (bit 5 of sie)
    set_csr(sie, MIP_STIP);
    ticks = 0;
    clock_set_next_event();
    cprintf("++ setup timer interrupts\n");
}

/* intr_enable - enable irq interrupt */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* intr_disable - disable irq interrupt */
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }

/* print_regs - print the general purpose registers */
void print_regs(struct pushregs *gpr) {
    cprintf("  zero     0x%016llx\n", gpr->zero);
    cprintf("  ra       0x%016llx\n", gpr->ra);
    cprintf("  sp       0x%016llx\n", gpr->sp);
    cprintf("  gp       0x%016llx\n", gpr->gp);
    cprintf("  tp       0x%016llx\n", gpr->tp);
    cprintf("  t0       0x%016llx\n", gpr->t0);
    cprintf("  t1       0x%016llx\n", gpr->t1);
    cprintf("  t2       0x%016llx\n", gpr->t2);
    cprintf("  s0       0x%016llx\n", gpr->s0);
    cprintf("  s1       0x%016llx\n", gpr->s1);
    cprintf("  a0       0x%016llx\n", gpr->a0);
    cprintf("  a1       0x%016llx\n", gpr->a1);
    cprintf("  a2       0x%016llx\n", gpr->a2);
    cprintf("  a3       0x%016llx\n", gpr->a3);
    cprintf("  a4       0x%016llx\n", gpr->a4);
    cprintf("  a5       0x%016llx\n", gpr->a5);
    cprintf("  a6       0x%016llx\n", gpr->a6);
    cprintf("  a7       0x%016llx\n", gpr->a7);
    cprintf("  s2       0x%016llx\n", gpr->s2);
    cprintf("  s3       0x%016llx\n", gpr->s3);
    cprintf("  s4       0x%016llx\n", gpr->s4);
    cprintf("  s5       0x%016llx\n", gpr->s5);
    cprintf("  s6       0x%016llx\n", gpr->s6);
    cprintf("  s7       0x%016llx\n", gpr->s7);
    cprintf("  s8       0x%016llx\n", gpr->s8);
    cprintf("  s9       0x%016llx\n", gpr->s9);
    cprintf("  s10      0x%016llx\n", gpr->s10);
    cprintf("  s11      0x%016llx\n", gpr->s11);
    cprintf("  t3       0x%016llx\n", gpr->t3);
    cprintf("  t4       0x%016llx\n", gpr->t4);
    cprintf("  t5       0x%016llx\n", gpr->t5);
    cprintf("  t6       0x%016llx\n", gpr->t6);
}

/* print_trapframe - print the whole trapframe */
void print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%016llx\n", tf->status);
    cprintf("  epc      0x%016llx\n", tf->epc);
    cprintf("  badvaddr 0x%016llx\n", tf->badvaddr);
    cprintf("  cause    0x%016llx\n", tf->cause);
}

/* interrupt_handler - handle an interrupt (scause has the high bit set) */
void interrupt_handler(struct trapframe *tf) {
    intptr_t cause = (tf->cause << 1) >> 1;
    switch (cause) {
        case IRQ_U_SOFT:
        case IRQ_S_SOFT:
        case IRQ_H_SOFT:
        case IRQ_M_SOFT:
            cprintf("Software interrupt\n");
            break;
        case IRQ_U_TIMER:
        case IRQ_S_TIMER:
        case IRQ_H_TIMER:
        case IRQ_M_TIMER:
            // the timer has fired: schedule the next one and count
            clock_set_next_event();
            if (++ticks % TICK_NUM == 0) {
                print_ticks();
            }
            break;
        case IRQ_U_EXT:
        case IRQ_S_EXT:
        case IRQ_H_EXT:
        case IRQ_M_EXT:
            cprintf("External interrupt\n");
            break;
        default:
            print_trapframe(tf);
            break;
    }
}

/* exception_handler - handle a synchronous exception */
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
        case CAUSE_MISALIGNED_FETCH:
        case CAUSE_FAULT_FETCH:
        case CAUSE_ILLEGAL_INSTRUCTION:
        case CAUSE_BREAKPOINT:
        case CAUSE_MISALIGNED_LOAD:
        case CAUSE_FAULT_LOAD:
        case CAUSE_MISALIGNED_STORE:
        case CAUSE_FAULT_STORE:
        case CAUSE_USER_ECALL:
        case CAUSE_SUPERVISOR_ECALL:
        case CAUSE_HYPERVISOR_ECALL:
        case CAUSE_MACHINE_ECALL:
        default:
            print_trapframe(tf);
            break;
    }
}

/* trap_dispatch - dispatch based on what type of trap occurred */
void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        // interrupts
        interrupt_handler(tf);
    } else {
        // exceptions
        exception_handler(tf);
    }
}

/* trap - the C entry point of the trap handling, called by trapentry.S */
void trap(struct trapframe *tf) {
    trap_dispatch(tf);
}

/* trap_init - point stvec at the assembly trap entry */
void trap_init(void) {
    extern void __alltraps(void);
    write_csr(stvec, __alltraps);
}
