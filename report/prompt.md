### 1.功能模块：Trap 上下文恢复与 Trap 入口功能（设计提示词借助 ai 完成）

**负责人：** 全员

```markdown
[PROMPT]

任务：完成 `kern/trap/trapentry.S` 中的 Trap 上下文恢复与 Trap 入口功能，使系统能够保存 Trap 上下文、调用 C 语言处理函数，并在处理完成后恢复上下文返回原执行流。

操作要求：直接修改项目中的 `kern/trap/trapentry.S`，完成 `RESTORE_ALL` 和 `__alltraps` 的实现。保留文件中已有代码，并将实现放在相应位置。不要只输出代码片段。

边界：只修改完成该功能所必需的汇编代码。不得修改 `SAVE_ALL`、`struct trapframe`、`trap()` 的函数签名或 C 语言 Trap 处理逻辑。不要引入与现有 Trap frame 布局不兼容的数据结构或约定。

[RELY]

`kern/trap/trapentry.S` 中已有 `SAVE_ALL`，负责把处理器状态保存到当前内核栈上的 Trap frame。该帧共占用 `36 * REGBYTES` 字节，其中 `REGBYTES`、`LOAD` 和 `STORE` 由 `riscv.h` 定义。

Trap frame 的槽位与 `struct trapframe` 保持一致：槽位 0 至 31 对应通用寄存器 x0 至 x31，槽位 32 保存 `sstatus`，槽位 33 保存 `sepc`，槽位 34 保存 `sbadaddr`，槽位 35 保存 `scause`。x2（sp）有特殊保存方式：`SAVE_ALL` 将 Trap 发生前的 sp 保存到槽位 2，因此恢复时必须在不再需要 Trap frame 基址后恢复 sp。

C 语言处理接口为 `void trap(struct trapframe *tf)`。Trap frame 的基址应按 RISC-V 调用约定作为 `trap()` 的第一个参数传递。`trap()` 返回后，执行流应进入现有的 `__trapret`，再通过 `sret` 返回。

[GUARANTEE]

必须实现或完善以下汇编功能：

`RESTORE_ALL`：从当前 Trap frame 恢复处理器状态。

`__alltraps`：建立到 `trap(struct trapframe *tf)` 的调用关系。

允许添加完成上述功能所需的汇编指令或局部标签，不得改变对外接口。

[SPECIFICATION]

Trap 上下文恢复与入口功能

Pre-Condition：Trap 已发生，`SAVE_ALL` 已将处理器状态保存到当前内核栈上的 Trap frame；当前 sp 指向该 Trap frame。

Post-Condition：Trap frame 被正确传递给 `trap()`。处理函数返回后，处理器状态从 Trap frame 恢复，执行 `sret` 返回到 Trap 发生前的执行流。

Case 1：发生 Trap 时，入口代码应保留 `SAVE_ALL` 创建的 Trap frame，并将其地址作为 `trap()` 的第一个参数传入。`trap()` 正常返回后，执行流应进入 `__trapret`。

Case 2：执行上下文恢复时，应恢复 Trap frame 中保存的特权状态和通用寄存器。x0 保持硬连线零值；x2（sp）应根据槽位 2 中保存的原始值恢复，并确保恢复后不再依赖 Trap frame 基址。

Case 3：上下文恢复完成后，应执行 `sret`，由处理器根据恢复后的特权状态和返回地址继续 Trap 发生前的执行流。

Requirements：

`RESTORE_ALL` 必须严格匹配 `SAVE_ALL` 的 36 个槽位布局，并兼容 `REGBYTES`、`LOAD` 和 `STORE` 的定义。

调用 `trap()` 时应遵守 RISC-V 调用约定，并保持 Trap frame 内容可供返回路径恢复。

不得修改 `SAVE_ALL`、Trap frame 结构、`trap()` 函数签名或 C 语言 Trap 处理逻辑。
```

### 2.功能模块：中断与异常处理功能（设计提示词借助 ai 完成）

**负责人：** 全员

```
[PROMPT]

任务：在当前 Lab 3 工程中实现中断与异常处理功能。在 kern/trap/trap.c 的 lab3 function2 YOUR CODE 位置实现 interrupt_handler 和 exception_handler，使现有 trap_dispatch() 能正确处理监督者时钟中断、非法指令异常、断点异常，并为其他 Trap 输出诊断信息。

需要实现的函数：

void interrupt_handler(struct trapframe *tf);

void exception_handler(struct trapframe *tf);

操作要求：

先阅读当前工程中的 kern/trap/trap.c、kern/trap/trap.h、kern/trap/trapentry.S、kern/driver/clock.c、kern/driver/clock.h、libs/riscv.h、libs/sbi.h，以及实际存在的 Lab 3 测试文件和测试脚本，以工程定义和测试要求为准。

直接修改项目中的实际文件，不要只输出代码示例。修改范围仅限 kern/trap/trap.c 中本模块所需的代码及必要的内部定义。保留现有代码和 lab3 标记注释。

不得修改模块一中的 SAVE_ALL、RESTORE_ALL、__alltraps，不得修改 trap_dispatch()、trap()、测试文件或现有函数签名。

若实现关机需要调用 sbi_shutdown()，根据当前工程中的头文件声明正确包含 libs/sbi.h。不得假设工程中存在实际未定义的宏、函数或接口。

RELY

trap_dispatch() 已根据 tf->cause 的最高位区分中断与异常，并将 Trap frame 传给对应处理函数。

tf->cause 保存 Trap 原因，tf->epc 保存 Trap 返回时使用的指令地址。异常处理函数修改 tf->epc 后，返回路径会将更新后的地址写回 sepc，再通过 sret 返回。

clock_set_next_event() 用于设置下一次时钟事件，ticks 用于记录时钟中断次数。TICK_NUM 和 print_ticks() 已在 trap.c 中提供，应沿用现有统计输出逻辑。

中断号和异常号以当前工程 libs/riscv.h 中的 IRQ_S_TIMER、CAUSE_ILLEGAL_INSTRUCTION、CAUSE_BREAKPOINT 等定义为准。

print_trapframe(tf) 可用于输出完整 Trap frame，sbi_shutdown() 可用于关机；使用前应核实它们在当前工程中的声明和用法。

SPECIFICATION

interrupt_handler(tf)

Pre-Condition：tf 指向有效的 Trap frame，当前 Trap 原因是中断。

Post-Condition：监督者时钟中断能够持续产生；时钟计数和统计输出符合工程及实验要求。其他已识别的中断能输出相应提示。无法识别的中断会输出诊断信息并调用 print_trapframe(tf)。

Case 1：收到 Supervisor Timer 中断时，安排下一次时钟事件，并使 ticks 恰好增加一次。每当 ticks 达到 TICK_NUM 的整数倍时，沿用现有 print_ticks() 输出统计信息。

Case 2：收到当前工程能够识别、但实验没有要求专门处理的其他中断时，区分其中断类型并输出适当提示。不得对这些中断执行未经要求的状态修改。

Case 3：收到无法识别的中断时，输出中断原因，并调用 print_trapframe(tf) 输出完整 Trap frame。

Requirements：

比较中断原因前，先正确区分并剥离 tf->cause 的中断标志位，再将原因编号与 IRQ_S_TIMER 等中断号比较。

每次 Supervisor Timer 中断都必须调用 clock_set_next_event()，避免只收到一次时钟中断。每次中断只增加一次 ticks。

保留 print_ticks() 的现有输出格式；测试要求能够观察到 100 ticks。若当前工程或测试对时钟中断次数另有要求，应以实际代码和测试为准。

先查找工程代码、实验资料和测试配置中是否规定累计统计输出次数后的关机阈值。只有找到明确规定值时才据此调用 sbi_shutdown()。若工程与实验资料均未给出阈值，不得自行猜测或添加任意数值，并在最终说明中指出缺少该信息。

exception_handler(tf)

Pre-Condition：tf 指向有效的 Trap frame，当前 Trap 原因是异常。

Post-Condition：非法指令异常和断点异常能够输出实验要求的信息，并更新 tf->epc，使 sret 返回后继续执行异常指令之后的位置。其他异常通过 print_trapframe(tf) 输出完整诊断信息。

Case 1：收到非法指令异常时，输出包含 Illegal instruction at 的信息。根据触发指令的实际长度推进 tf->epc，避免返回后重复执行该指令。

Case 2：收到断点异常时，按工程或实验资料规定的格式输出信息，并根据断点指令的实际长度推进 tf->epc，避免返回后再次触发同一断点异常。

Case 3：收到其他异常时，调用 print_trapframe(tf) 输出完整 Trap frame，不执行实验未要求的异常恢复操作。

Requirements：

指令长度必须依据 RISC-V 指令编码判断。至少区分 16 位压缩指令与 32 位指令，不能对所有非法指令或断点异常都使用同一个固定的 tf->epc 增量。

异常输出格式以当前工程和实验资料中能够确认的要求为准。若无法确认具体格式，应采用与现有代码风格一致且包含必要原因和地址的信息，并在最终说明中指出依据。

实现完成后，检查修改范围，确认没有改动本模块之外的函数、文件、接口或测试。若运行测试，应说明实际执行的测试命令和结果。
```


