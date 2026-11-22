#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>

// process's state in his life cycle
enum proc_state
{
    PROC_UNINIT = 0, // uninitialized
    PROC_SLEEPING,   // sleeping
    PROC_RUNNABLE,   // runnable(maybe running)调度器通过 current 判断哪个线程正在运行
    PROC_ZOMBIE,     // almost dead, and wait parent proc to reclaim his resource
};                   //僵尸状态，值为 3。表示线程已经退出，但仍保留部分管理信息，等待父进程回收。

struct context
{
    uintptr_t ra;   //保存返回地址寄存器 ra。普通线程被切走时，
                    // 记录 switch_to() 的调用返回地址；以后切回来，可以从调用之后继续执行。
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};
//线程切换在一个函数当中，编译器会自动帮助我们生成保存和恢复调用者保存寄存器的代码

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct proc_struct
{
    enum proc_state state;        // Process state
    int pid;                      // Process ID
    int runs;                     // the running times of Proces
    uintptr_t kstack;             // Process kernel stack  初始栈顶位于 kstack + KSTACKSIZE 附近，而不是 kstack 本身
    volatile bool need_resched;   // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;   // the parent process
    struct mm_struct *mm;         // Process's memory management field指向地址空间管理结构，用于描述虚拟内存区域等信息。
    struct context context;       // Switch here to run processcontext 帮助线程恢复到切换点；trapframe 帮助线程从陷阱恢复路径进入或恢复目标执行现场
    struct trapframe *tf;         // Trap frame for current interrupt
    uintptr_t pgdir;              // 页目录的基址，每个进程的页表根节点的物理地址
    uint32_t flags;               // Process flag
    char name[PROC_NAME_LEN + 1]; // Process name
    list_entry_t list_link;       // Process link list
    list_entry_t hash_link;       // Process hash list
};

#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)   //利用成员在结构体中的偏移，从成员地址反推出整个线程控制块的地址

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);

#endif /* !__KERN_PROCESS_PROC_H__ */
