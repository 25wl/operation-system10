#include <stdio.h>
#include <string.h>
#include <sbi.h>
#include <trap.h>

int kern_init(void) __attribute__((noreturn));

int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);

    // set up the trap entry, enable and arm the timer interrupt
    trap_init();
    clock_init();
    intr_enable();

    while (1)
        ;
}
