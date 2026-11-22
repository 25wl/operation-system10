### 1.功能模块：[虚拟内存页操作功能]

```markdown

[按照提示词模板，给出具体的提示词]
[PROMPT]

在现有 riscv64-ucore Lab4 工程中，完成练习功能模块一：
虚拟内存页操作。

请先阅读 kern/mm/pmm.c、kern/mm/pmm.h、页表相关定义及官方模块一测试，
理解现有多级页表、引用计数和 TLB 接口，再实际实现代码。

仅在 kern/mm/pmm.c 的功能模块一占位处实现 page_insert() 和
page_remove()。不要实现其他成员负责的线程创建、调度或 Challenge，
不要修改官方测试、降低断言要求或通过固定输出伪造通过结果。

[RELY]

工程提供以下接口和定义，具体签名以实际源码为准：

- get_pte(pgdir, la, create)：查找目标页表项，必要时创建中间页表。
- pte_create(ppn, perm)：构造页表项，内部设置 PTE_V。
- pte2page()、pde2page()、page2ppn()：完成页表项和物理页描述符转换。
- page_ref_inc()、page_ref_dec()：维护物理页引用计数。
- free_page()：释放物理页。
- tlb_invalidate(pgdir, la)：使相应虚拟地址的 TLB 翻译失效。
- PDX1、PTE_V、pde_t、pte_t、struct Page 和 E_NO_MEM 等已有定义。

当前 get_pte() 可能在成功分配第一个中间页表、但分配下一级页表失败时
返回 NULL，并留下本次新建的中间页表。必须检查并处理这一失败路径，
不能仅返回错误而忽略本次调用新增的资源。

正常调用遵守现有框架前置条件：页表结构合法，传入 Page 有效，
调用期间不存在未同步的并发页表修改。

[GUARANTEE]

提供以下接口，保持函数签名不变：

int page_insert(pde_t *pgdir, struct Page *page,
                uintptr_t la, uint32_t perm);

void page_remove(pde_t *pgdir, uintptr_t la);

复用已有页表、引用计数和 TLB 接口，不修改 get_pte()、公共数据结构、
官方测试或其他成员模块。

page_insert() 成功返回 0，无法创建所需页表时返回 -E_NO_MEM。
page_remove() 为幂等删除接口，未映射地址不应被当作错误。

[SPECIFICATION]

1. page_insert()

- 使用 get_pte(pgdir, la, 1) 获取目标页表项。
- 获取失败时，不增加目标物理页的引用计数，不破坏已有映射。
- 如果失败前本次调用新建了中间页表，应回滚该新增页表并归还资源；
  调用前已经存在的中间页表必须保留。
- 建立新映射时增加目标物理页的引用计数。
- 替换已有映射前，先持有新页引用，再删除旧映射，
  避免同一物理页重映射时因引用计数暂时归零而被错误释放。
- 使用 pte_create() 设置目标物理页和调用者要求的权限。
- 同页重映射后引用计数不变，权限按新请求更新。
- 替换其他页时，正确减少旧页引用，旧页引用归零则释放。
- 更新完成后调用 tlb_invalidate()。

2. page_remove()

- 使用 get_pte(pgdir, la, 0) 查找，不分配新的中间页表。
- 页表项不存在或无效时直接返回。
- 对有效映射，取得原物理页，清除页表项并使对应 TLB 翻译失效。
- 减少原物理页引用计数，归零时调用 free_page()。
- 重复删除不得导致重复释放或引用计数下溢。
- 普通删除不额外回收空中间页表，保持现有框架约定。

3. 验证

- 在现有 WSL、课程工具链和 QEMU 环境中实际构建和运行。
- 原样运行可用的官方模块一测试及现有页表自检。
- 补充验证同页权限更新、异页替换、多地址引用、重复删除、
  页表分配失败与回滚，以及实际访存中的 TLB 更新。
- 若完整工程被其他未完成模块或依赖阻断，只在临时副本隔离验证，
  测试桩不得进入正式提交源码。
- 如实区分完整工程构建结果与模块一隔离测试结果，
  不将模块一通过表述为整个 Lab4 或完整评分通过。
```

### 2.功能模块：[进程创建及初始化功能]

```markdown

[PROMPT]
请阅读 Lab 4 指导书中“功能模块二：进程创建及初始化功能”和“创建并执行内核线程”的内容，结合当前仓库代码，实现该模块。项目路径为 D:\develop\os\RISCV\lab4\lab4_new。

本次只修改 kern/process/proc.c 中“//LAB4 function2 YOUR CODE”对应的区域。不要实现功能模块一或模块三，也不要改动调度、上下文切换和 trap 代码。

[RELY]
实现前请先核对以下现有定义与调用关系，不要凭其他版本的 ucore 代码猜测字段或接口：

- kern/process/proc.h：struct proc_struct、struct context、进程状态及函数声明。
- kern/trap/trap.h：struct trapframe 的实际字段。
- kern/process/entry.S：kernel_thread_entry 如何使用 s0、s1。
- kern/trap/trapentry.S：forkrets 如何恢复 trapframe。
- kern/process/proc.c：proc_list、hash_list、current、idleproc、initproc、nr_process，以及 get_pid、hash_proc、setup_kstack、put_kstack、forkret 等现有辅助函数。
- kern/mm/pmm.h、kern/mm/memlayout.h：CLONE_VM、内核栈大小及页表相关定义。
- kern/schedule/sched.c：wakeup_proc 的状态前提。

[ SPECIFICATION ]
请完成且仅完成以下六个函数：

1. alloc_proc(void)
Pre：无需已有进程。
Post：成功时返回已分配、基础字段全部安全初始化的 PCB；分配失败返回 NULL。
Requirements：初始化状态、PID、运行次数、内核栈、调度标记、父进程、mm、context、tf、pgdir、flags、名称和两个链表节点。不得留下会被后续创建流程误用的未初始化字段。

2. proc_init(void)
Pre：物理内存管理和必要的内核基础设施已初始化。
Post：初始化进程链表及 PID 哈希表；创建并设置 idleproc，使 current 指向它；再创建 initproc，设置名称为 "init"。
Requirements：idleproc 代表当前正在执行的现场，不通过 do_fork 创建。正确设置其 PID、状态、内核栈、页表和调度标记，并更新进程计数。通过 kernel_thread 创建执行 init_main 的 initproc，传入指导书预期的问候参数；检查创建结果及关键不变量。

3. kernel_thread(fn, arg, clone_flags)
Pre：fn 是内核线程入口函数。
Post：成功返回新线程 PID；失败返回 do_fork 的负错误码。
Requirements：在临时 trapframe 中按本仓库的 RISC-V 字段布局放置函数入口和参数，使 kernel_thread_entry 能从 s0、s1 取得它们；正确设置首次返回地址及 sstatus，再调用 do_fork。说明这些寄存器设置如何与 entry.S 对应。

4. do_fork(clone_flags, stack, tf)
Pre：tf 指向待复制的首次运行现场。
Post：成功创建、登记并唤醒新线程，返回其 PID；失败返回恰当的负错误码，且不泄漏已取得的资源。
Requirements：检查进程数量上限；依次取得 PCB、内核栈并处理地址空间，再设置父进程、PID 和首次运行现场。仅在对象完整可用后，将其挂入进程链表和 PID 哈希表、更新 nr_process，并调用 wakeup_proc。注意 wakeup_proc 要求目标进程此前既不是 PROC_RUNNABLE，也不是 PROC_ZOMBIE。失败路径应按已完成的步骤逆序释放资源；失败对象不得留在全局链表中，nr_process 不得错误增加。必要的全局结构更新须按现有中断保护接口处理。

5. copy_thread(proc, esp, tf)
Pre：proc 已有有效内核栈，tf 有效。
Post：新线程的 trapframe 位于自身内核栈内，context 可用于第一次上下文切换。
Requirements：复制 trapframe，设置子线程返回值为 0，按 esp 设置其栈指针；设置 context.ra 为 forkret、context.sp 为新 trapframe 的地址。核对内核栈边界，不要让 trapframe 指向调用者的临时变量。

6. copy_mm(clone_flags, proc)
Pre：proc 已完成 PCB 基础初始化。
Post：符合本实验阶段的地址空间处理要求。
Requirements：当前仅创建内核线程，不实现用户地址空间复制；保持 mm 等字段的合法状态并返回成功。保留 clone_flags 参数及完整接口，供后续实验扩展，不要虚构尚不存在的地址空间复制逻辑。

[GUARANTEE]
- 新线程首次执行链路应为：context.ra → forkret → forkrets → __trapret → kernel_thread_entry → fn(arg)。
- initproc 创建后应可被后续“功能模块三”的调度代码运行；本次不实现 cpu_idle、schedule、proc_run 或 switch_to。
- 保留 LAB4 标记。不要修改其他实验模块、测试脚本或指导书文件。
- 完成后说明六个函数各自做了什么，并给出实际执行过的编译或测试命令及结果。若模块一或模块三尚未完成，明确指出完整启动或 make grade 受这些前置模块影响，不要把未运行的测试写成通过。
```

### 3.功能模块三： [进程调度]

```markdown
[PROMPT]

任务：
完成内核线程的调度与 CPU 执行权交接能力，使已经创建并就绪的线程能够真正运行，并支持线程暂停后恢复执行。模块应形成“空闲循环检测调度请求 → 选择可运行线程 → 原子切换当前线程和地址空间 → 保存与恢复寄存器上下文”的完整协作链路，使 initproc 能够从 idleproc 手中接管 CPU。

涉及文件及职责：
kern/process/proc.c：实现空闲线程执行主体，以及线程执行权与地址空间切换。
kern/schedule/sched.c：实现基于进程链表的循环调度选择。
kern/process/switch.S：实现底层寄存器上下文保存与恢复。注意真实文件名为大写 .S。

操作要求：
必须在项目中修改实际文件，而不是仅展示代码。保留已有逻辑，在 LAB4 function3 YOUR CODE 对应位置补全实现；允许添加必要的头文件引用和模块内部 static 辅助函数。不得改变公开接口、struct context 布局或 struct proc_struct 定义。

本任务只负责已有线程的调度和切换，不负责线程创建、内核栈分配、trapframe 构造、退出回收、时钟中断调度策略、用户进程或多核同步。不得修改功能模块二、内存管理器、陷阱恢复代码和评分脚本来绕过缺失依赖或测试。

输出要求：
以工程真实代码为准，使用下面提供的上下文和规格完成实现。交付时说明修改内容、验证结果及未完成验证的具体原因。若前置模块尚未实现，应明确报告依赖缺失，不得声称已经验证完整启动链路。

[RELY]

目标环境与线程上下文：

工程使用 RV64 和 Sv39。kern/process/proc.h 中的真实上下文布局为：

struct context
{
    uintptr_t ra;
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

RV64 下每个字段占 8 字节，字段偏移依次为 0、8、16……104。该布局是 C 与汇编之间的接口约定，不得调整。

struct proc_struct 中与本任务直接相关的字段为：
state：线程状态。
runs：被调度选中的次数。
need_resched：是否需要重新调度。
context：线程切换上下文。
tf：线程的陷阱帧指针。
pgdir：根页表物理地址。
list_link：普通线程在 proc_list 中的链表节点。

完整类型和布局以 kern/process/proc.h 为准，不得重新定义结构体。

可运行状态为 PROC_RUNNABLE，该状态同时包含当前正在运行的线程；工程没有单独的 PROC_RUNNING 状态。

调度数据与链表接口：

extern list_entry_t proc_list;
extern struct proc_struct *idleproc, *initproc, *current;

可使用 list_next() 遍历链表，使用 le2proc(le, list_link) 将普通链表节点转换为线程对象。

proc_list 是带哨兵头结点的循环双向链表，通过 list_link 连接普通线程。前置初始化应保证：
idleproc 有效，但不作为普通节点加入 proc_list。
current 有效；非空闲的当前线程位于 proc_list 中。
链表和线程状态已经初始化。

禁止对链表头 &proc_list 使用 le2proc()。

中断保护接口：

kern/sync/sync.h 提供：

local_intr_save(flag);
local_intr_restore(flag);

flag 应为当前调用栈上的 bool 局部变量。保存操作记录进入临界区前的中断使能状态并关闭中断；恢复操作按保存状态恢复，不能无条件开启中断。

这两个接口允许嵌套使用。本任务基于单核执行环境，关闭本地中断用于保护调度切换临界区。

地址空间接口：

可使用以下已有资源：

extern uintptr_t boot_pgdir_pa;

SATP_MODE_SV39 的值为 8。
RISCV_PGSHIFT 的值为 12。

read_csr(reg);
write_csr(reg, val);
flush_tlb();

线程的 pgdir 应由前置模块设置为有效、按页对齐的根页表物理地址；本实验内核线程使用共享内核页表，通常为 boot_pgdir_pa。不得将内核虚拟地址直接写入 satp，也不得将已经是物理地址的 pgdir 再次转换。

Sv39、ASID 为零时，目标 satp 的编码关系为：

((uintptr_t)SATP_MODE_SV39 << 60) | (proc->pgdir >> RISCV_PGSHIFT)

SATP_MODE_SV39 本身只是模式编号，不能直接与物理页号相或。模式字段必须以 64 位宽度计算。

参考 kern/mm/pmm.c 中 enable_paging() 的写法。libs/riscv.h 中的 lsatp() 使用 Sv32 编码且参数为 unsigned int，本任务禁止使用。

libs/riscv.h 还提供汇编宏 LOAD、STORE、REGBYTES，RV64 下分别对应 64 位加载、存储和 8 字节寄存器宽度。

首次运行链路：

前置模块应已经完成以下准备：
新线程的 context.ra 指向 forkret。
context.sp 与预先构造的内核栈、trapframe 布局匹配。
trapframe 包含线程入口、参数及正确的特权状态。

已有恢复链路为：

switch_to → forkret → forkrets(current->tf) → __trapret → kernel_thread_entry → 线程入口函数

首次进入的新线程通过既有 trapframe 恢复路径建立中断状态；此前运行过的线程则在恢复原调用栈后继续执行自己的中断恢复逻辑。

[GUARANTEE]

必须实现以下接口，签名保持一致：

kern/process/proc.c：
void cpu_idle(void);
void proc_run(struct proc_struct *proc);

kern/schedule/sched.c：
void schedule(void);

kern/process/switch.S：
void switch_to(struct context *from, struct context *to);

保留 proc.h 中 cpu_idle 的 noreturn 声明。

汇编导出符号必须为 switch_to，与 C 声明一致。

允许添加必要的模块内部 static 辅助函数，但不得新增对外调度接口或改变数据布局。

[SPECIFICATION]

cpu_idle：空闲等待与调度请求响应

Pre-Condition：
当前线程为已经初始化完成的 idleproc。
调度模块可用，current 与 idleproc 一致。

Post-Condition：
函数持续执行空闲循环，永不返回。
每次从调度调用恢复后，继续检测当前空闲线程的重调度标志。

Case 1：存在调度请求
检测到 current->need_resched 为真时，发起一次调度；若 CPU 执行权转移到其他线程，空闲线程暂停在该调用链上，直到以后重新获得执行权。

Case 2：不存在调度请求
继续等待，不主动发起调度，不修改线程状态和运行次数。

Requirements：
不得永久关闭中断。
不引入睡眠队列或依赖新的唤醒机制。
本任务采用持续检测的空闲循环，不额外引入 wfi 等等待协议。

schedule：循环选择可运行线程

Pre-Condition：
current、idleproc 和循环链表均有效。
线程加入和移出链表的行为满足前置模块约定。

Post-Condition：
原当前线程的 need_resched 已清零。
完成一次有界的调度选择：优先选择链表中的可运行线程，找不到时选择 idleproc。
被选线程的 runs 恰好增加一次，包括再次选中当前线程或回退到空闲线程的情况。
仅当被选线程与当前线程不同时，触发实际线程切换。
当前调用最终恢复并返回时，中断使能状态与进入本次调用前一致。

Case 1：当前线程为空闲线程
从 proc_list 的第一个普通节点开始，按链表顺序选择第一个 PROC_RUNNABLE 线程。

Case 2：当前线程为普通线程
从当前线程的后继节点开始循环扫描，跳过哨兵头结点；允许绕回表头，并在检查完其他节点后检查当前线程自身。选择遇到的第一个可运行线程。

Case 3：当前线程是唯一可运行的普通线程
循环检查后重新选中当前线程，增加其 runs，但不执行底层上下文切换。

Case 4：链表为空或所有普通线程均不可运行
选择 idleproc，不得选择睡眠、未初始化或僵尸线程，不得无限遍历。

Requirements：
从清除重调度标志、扫描链表、更新计数到触发切换，均处于保存中断状态后的临界区。
不改变链表连接关系，不分配或释放线程资源。
扫描最多覆盖一轮普通节点，时间复杂度为 O(n)。
调度器与 proc_run 的嵌套中断保护必须保持正确。

proc_run：原子交接执行权与地址空间

Pre-Condition：
proc 为非空、有效的目标线程；需要切换时其状态为 PROC_RUNNABLE。
current 有效。
目标上下文及页表物理地址有效。
目标页表保留切换代码、相关内核数据及切换期间所需内核栈的映射。

Post-Condition：
实际切换时，旧线程的上下文已保存，目标线程的上下文已恢复。
目标线程获得执行权时，current 指向它，satp 使用它的根页表，旧地址转换缓存已按要求刷新。
旧线程以后再次被切回时，从原先的切换调用之后继续执行，并恢复该次调用保存的中断状态。

Case 1：目标就是当前线程
直接返回，不写入页表寄存器、不刷新 TLB、不调用底层切换，也不修改 runs。

Case 2：目标为另一线程
在关闭中断的临界区内保留旧线程指针，更新 current，安装目标线程的 Sv39 页表并刷新 TLB，然后完成旧上下文到目标上下文的交接。

Requirements：
不得在 current、有效页表和实际执行上下文尚未完成交接时开启中断。
satp 必须使用目标 pgdir 的物理页号，MODE 为 Sv39，ASID 为零。
页表更新及 TLB 刷新必须发生在目标上下文开始执行之前。
不重新构造目标上下文或 trapframe，不直接调用目标线程入口。
不自行选择线程，不重复增加 runs。
旧线程通常要等以后被重新调度，才继续完成当前 proc_run 调用，不能假设切换后立即返回。

switch_to：寄存器执行现场迁移

Pre-Condition：
遵循 RISC-V C 调用约定：a0 指向 from，a1 指向 to。
两个上下文对象有效、可访问且互不重叠。
调用者已经完成线程选择、页表切换和中断保护。
目标 ra、sp 已初始化，且目标栈满足工程 ABI 对齐要求。

Post-Condition：
进入本函数时的 ra、sp、s0 至 s11 被完整保存到 from。
同一组寄存器从 to 恢复，所有偏移和宽度与 struct context 严格一致。
控制流转移到目标上下文保存的 ra，使用目标 sp 继续执行。

Case 1：恢复此前运行过的线程
恢复其保存的调用现场，使其从此前调用 switch_to 之后继续执行。

Case 2：首次运行新线程
通过预置的返回地址进入 forkret，沿已有 trapframe 恢复链路进入线程入口，不添加单独的首次运行分支。

Requirements：
先完成全部旧上下文保存，再恢复目标上下文。
RV64 下使用 64 位存取，不能截断寄存器。
不要求保存调用者保存寄存器，不负责保存完整 trapframe。
不修改 current、线程状态、运行次数、页表或中断状态。
恢复目标栈指针后，不得继续访问旧栈上的临时数据。
保持共享内核环境使用的 gp、tp 约定，不随意覆盖它们。

模块验收：

通过现有构建、测试、调试器观察或临时测试环境验证以下要求，不得修改正式测试逻辑来制造通过结果。

接口与布局：
四个接口符号正确；汇编存取偏移覆盖 ra、sp、s0 至 s11，与 C 布局逐项一致。

选择顺序：
构造多个普通线程，验证从当前线程后继开始选择、跳过非 runnable 节点、跨哨兵回绕，以及最后允许重新选中当前线程。

兜底行为：
空链表和全部普通线程不可运行时均选择 idleproc，扫描能够结束。

计数与标志：
每次调度清除原当前线程的 need_resched，仅被选线程的 runs 增加一次。

同线程路径：
直接请求运行当前线程时，不触发页表重写和上下文切换。

中断状态：
分别从中断开启和关闭状态进入，验证调用恢复后保持原状态；检查嵌套保护不会提前开启中断。

页表状态：
目标开始执行时，current 正确，satp 的 MODE 为 8、ASID 为零、PPN 与目标 pgdir 对应；通过源码或反汇编确认页表写入后执行了 TLB 刷新。

恢复能力：
在可重复切换的测试线程中保存不同的被调用者保存寄存器值，验证多次切换后寄存器及栈上的局部状态仍能恢复。

首次执行：
前置模块完整时，验证 idleproc 向 initproc 交接执行权，能够进入 init_main 并输出其现有信息。

验收边界：
当前 do_exit() 的既有实现会触发 process exit!! 的 panic，不得将实现正常退出回收作为本模块要求。前置线程创建代码缺失时，应报告完整运行验证受阻，不得擅自补写其他模块。
```


