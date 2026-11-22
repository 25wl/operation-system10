#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;
//所有进程控制块的双向线性列表

#define HASH_SHIFT 10    //PID 哈希计算使用的位数
#define HASH_LIST_SIZE (1 << HASH_SHIFT) //1024
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0; //记录当前已有线程数量，包括 idleproc

void kernel_thread_entry(void);  //内核线程启动入口，负责从指定寄存器取出函数地址和参数，调用线程函数
void forkrets(struct trapframe *tf); //声明陷阱帧恢复入口，将栈指针指向传入的 trapframe，然后进入 __trapret
void switch_to(struct context *from, struct context *to);  //声明底层上下文切换接口

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);  //为寻找空闲 PID 留出空间
    struct proc_struct *proc;              //定义遍历时使用的线程指针
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;  //next_safe 表示继续递增候选 PID 时需要重新扫描的边界
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;       //PID 0 留给 idleproc
        goto inside;        //跳到重新扫描的入口，检查新的候选 PID 是否被占用
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);      //根据节点地址，得到它所属的线程控制块
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;       //丢弃旧的扫描边界，重新建立安全区间
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}


// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);  //恢复链路为：forkret → forkrets → __trapret → sret → kernel_thread_entry
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}


// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc)     //为指定线程分配内核栈
{
    struct Page *page = alloc_pages(KSTACKPAGE); //申请 KSTACKPAGE 个连续物理页，返回首个页面的 Page 描述符
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);  //先把 Page 描述符转换为对应内存的内核虚拟地址，再将地址存入 kstack
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE); 
}//把 kstack 的内核虚拟地址转换回 Page 描述符，然后释放相同数量的页面


// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int do_exit(int error_code)
{
    panic("process exit!!.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg)
{
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

//LAB4 function2 2512832
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        /* Clear context, pointers, flags and name before exposing the PCB. */
        memset(proc, 0, sizeof(*proc));
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->pgdir = boot_pgdir_pa;
        list_init(&proc->list_link);
        list_init(&proc->hash_link);
    }
    return proc;
}

static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    /* Lab 4 only creates kernel threads, all using the boot page table. */
    (void)clone_flags;
    proc->mm = NULL;
    proc->pgdir = boot_pgdir_pa;
    return 0;
}

static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    static_assert(sizeof(struct trapframe) <= KSTACKSIZE);
    assert(proc->kstack != 0 && tf != NULL);
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(*tf));
    assert((uintptr_t)proc->tf >= proc->kstack);
    assert((uintptr_t)proc->tf + sizeof(*tf) == proc->kstack + KSTACKSIZE);
    memcpy(proc->tf, tf, sizeof(*tf));
    proc->tf->gpr.a0 = 0;
    /* With no explicit stack, reuse the space below the saved trapframe. */
    proc->tf->gpr.sp = (esp != 0) ? esp : (uintptr_t)proc->tf;
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)proc->tf;
}

int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    if (nr_process >= MAX_PROCESS)
    {
        return -E_NO_FREE_PROC;
    }

    int ret = -E_NO_MEM;
    struct proc_struct *proc = alloc_proc();
    if (proc == NULL)
    {
        return ret;
    }
    if ((ret = setup_kstack(proc)) != 0)
    {
        goto bad_fork_cleanup_proc;
    }
    if ((ret = copy_mm(clone_flags, proc)) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }
    proc->parent = current;
    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag);
    /* Recheck before publishing in case creation was interrupted. */
    if (nr_process >= MAX_PROCESS)
    {
        local_intr_restore(intr_flag);
        ret = -E_NO_FREE_PROC;
        goto bad_fork_cleanup_kstack;
    }
    proc->pid = get_pid();
    hash_proc(proc);
    list_add(&proc_list, &proc->list_link);
    nr_process++;
    /* alloc_proc left state at PROC_UNINIT, as wakeup_proc requires. */
    wakeup_proc(proc);
    local_intr_restore(intr_flag);
    return proc->pid;

bad_fork_cleanup_kstack:
    /* copy_mm acquires no resources for kernel threads in this lab. */
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    return ret;
}

int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;
    memset(&tf, 0, sizeof(tf));
    /* entry.S moves s1 into a0, then jalr s0 invokes fn(arg). */
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    tf.epc = (uintptr_t)kernel_thread_entry;
    /* sret enters supervisor mode and enables interrupts via SPIE. */
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

void
proc_init(void)
{
    list_init(&proc_list);
    for (int i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(&hash_list[i]);
    }

    idleproc = alloc_proc();
    assert(idleproc != NULL);
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->pgdir = boot_pgdir_pa;
    idleproc->need_resched = 1;
    /* set_proc_name copies PROC_NAME_LEN bytes, so use padded names. */
    const char idle_name[PROC_NAME_LEN + 1] = "idle";
    set_proc_name(idleproc, idle_name);
    current = idleproc;
    nr_process++;

    int pid = kernel_thread(init_main, "Hello world!!", 0);
    assert(pid > 0);
    initproc = find_proc(pid);
    assert(initproc != NULL && initproc->pid == 1);
    const char init_name[PROC_NAME_LEN + 1] = "init";
    set_proc_name(initproc, init_name);
    assert(current == idleproc && idleproc->pid == 0);
    assert(initproc->parent == idleproc && initproc->state == PROC_RUNNABLE);
    assert(initproc->mm == NULL && initproc->pgdir == boot_pgdir_pa);
    assert(nr_process == 2);
}


//LAB4 function3  2512973
void
cpu_idle(void)   //空闲线程的执行主体，不正常返回
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}
//进入调度器，选择下一个可运行线程。如果选中其他线程，空闲执行流会暂停在这个调用链中；
// 以后重新获得 CPU 时，继续完成调用并回到循环。
void
proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag;   //在current栈上保存中断状态
        local_intr_save(intr_flag);//记录原中断使能状态，并关闭中断
        {
            struct proc_struct *prev = current;
            current = proc;

            /* Order current and page-table memory before installing satp. */
            asm volatile("" ::: "memory");//编译器内存屏障
            write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) |
                            (proc->pgdir >> RISCV_PGSHIFT));  //proc->pgdir 是根页表物理地址。RISCV_PGSHIFT 即右移 12 位
            flush_tlb();//执行实际的 RISC-V 地址转换同步指令
            asm volatile("" ::: "memory");
            switch_to(&prev->context, &proc->context); //进行实际的寄存器上下文交接
        }
        /* Reached when this thread is scheduled back onto its saved stack. */
        local_intr_restore(intr_flag);
        //通常不是旧线程刚切走就立刻执行，而是等旧线程以后再次被调度回来，恢复自己的栈和调用现场，才继续执行这一行。
        // 它恢复的是该线程进入这次 proc_run() 之前保存的中断状态
    }
}
//将当前运行的进程设置为要切换过去的进程。
//将页表切换到新进程的页表。
//使用 switch_to 切换到新进程的上下文。