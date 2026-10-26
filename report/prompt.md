##### 1. 实现物理内存探测与页描述符初始化功能

```
[PROMPT]

任务：修改项目中的 kern/mm/pmm.c，在 lab2 function1 对应位置实现 page_init() 函数，完成物理内存探测结果的读取、物理页描述符初始化，以及初始空闲物理页的登记。

根据设备树提供的物理内存信息，建立可管理物理页的描述符布局，将排除 OpenSBI、内核镜像和描述符数组占用空间后的完整空闲页交给当前 pmm_manager 管理。

操作要求：必须修改项目中的实际文件，而不是仅展示代码。实现前阅读相关头文件和现有初始化流程，使用当前工程提供的接口、宏和数据结构。仅补充本任务需要的实现，不修改其他功能模块。

输出要求：保留文件中的现有代码和实验标记，将实现插入对应位置。完成后简要说明内存范围、地址转换、描述符布局和空闲页登记的处理方式；如果进行了编译或测试，如实报告结果，未验证的内容不得声称通过。

[RELY]

以下名称以当前工程中的实际定义为准，使用前检查相关头文件，不重复定义已有内容。

宏定义
C
/* 页面大小、页号与对齐 */
PGSIZE
PGSHIFT
PPN(pa)
ROUNDUP(a, n)
ROUNDDOWN(a, n)
/* 内存布局与管理范围 */
DRAM_BASE
KERNBASE
KERNEL_BEGIN_PADDR
PHYSICAL_MEMORY_END
PHYSICAL_MEMORY_OFFSET
/* 内核虚拟地址转换为物理地址 */
PADDR(kva)
/* 页面保留状态 */
SetPageReserved(page)
数据结构与相关全局变量
C
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
/* 链接脚本提供的内核结束符号 */
extern char end[];
其中：

pages 指向物理页描述符数组，保存的是内核虚拟地址。
nbase 是描述符数组对应的起始物理页号。
npage 是可管理物理页号的排他上界，不是描述符数量。
描述符数量应与 npage - nbase 一致。
va_pa_offset 表示当前内核直映区域的虚拟地址与物理地址之差。
可用函数
C
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);
static void init_memmap(struct Page *base, size_t n);
static inline struct Page *pa2page(uintptr_t pa);
另外可使用框架提供的：

C
panic(...)
cprintf(...)
memset(...)
list_init(...)
init_memmap() 将指定的连续物理页交给当前内存管理器登记；调用前必须保证页数大于零，并满足该管理器对描述符初始状态的要求。

[GUARANTEE]

必须实现的接口：

C
void page_init(void);
该函数无参数、无返回值，由 pmm_init() 调用。如果遵循当前文件内部使用方式，可定义为：

C
static void page_init(void);
可以根据需要添加文件内部的辅助函数或局部定义，但不得修改已有公共接口，不得改变当前 pmm_manager 的选择。

[SPECIFICATION]

page_init
Pre-Condition：

内核已经进入初始化流程，pmm_init() 已选择并初始化当前物理内存管理器。
设备树初始化流程已执行，可以通过 get_memory_base() 和 get_memory_size() 获取解析结果；不得因此假定结果一定有效。
当前内核地址映射、物理内存管理上限和页面大小由工程已有宏定义。
链接符号 end 表示内核镜像布局结束处的虚拟地址。
Post-Condition：

对于有效且能够容纳内核及描述符数组的内存布局：

正确设置 va_pa_offset、npage 和 pages。
输出设备树探测到的物理内存范围。
为可管理物理页建立并初始化对应的 Page 描述符。
保留 OpenSBI、内核镜像和描述符数组等占用区域，不将其交给分配器。
将剩余完整空闲页登记给当前物理内存管理器。
Case 1：内存信息有效，且存在完整空闲页

读取并验证设备树内存范围，根据框架管理上限确定可管理范围。
将描述符数组安排在内核结束位置之后的适当对齐位置，确认数组完整落在可访问的实际 RAM 和管理范围内。
初始化描述符，保留已占用区域。
对可用范围进行页对齐，并调用 init_memmap() 登记剩余完整空闲页。
Case 2：设备树内存信息无效，或无法建立合法的初始化布局

对零值、地址计算溢出、内存范围无法容纳当前内核或描述符数组等情况进行检查。
使用 panic() 终止初始化，不得继续写入无效内存或登记错误空闲范围。
Case 3：布局有效，但没有剩余完整空闲页

完成有效布局所需的描述符初始化，保持相关页处于不可分配状态。
不调用 init_memmap()，尤其不得传入零页数量或越界范围。
Requirements：

严格区分内核虚拟地址、物理地址、物理页号和 Page 描述符地址。
使用当前框架规定的直映关系进行地址换算，不根据设备树内存起点任意修改映射偏移。
可管理结束位置不得超过设备树内存末尾及框架允许的管理上限；末尾不足一页的空间不得登记。
描述符数组和空闲区起点应正确对齐；描述符数量、大小和地址计算不得溢出。
在写入描述符数组前验证其位置与长度，不依赖原内存内容恰好为零。
交给 init_memmap() 的描述符必须满足当前管理器要求的初始保留状态。
使用真实存在的 pa2page() 等框架接口，不根据题干排版或提取错误编造接口。
保留已有代码、实验标记、测试和管理器选择；不实现或启用 Best-Fit、Buddy、SLUB等其他任务
```

##### 2.实现 Best-Fit 连续物理内存分配算法

```
[PROMPT]
任务：
参考 kern/mm/default_pmm.c 的完整 First-Fit 实现，完成 Best-Fit 物理内存管理模块，并让内核实际使用它。

修改范围：
1. kern/mm/best_fit_pmm.c：在 lab2、YOUR CODE 标记处补充内部状态、管理函数、必要的前置声明及 best_fit_pmm_manager 注册结构。
2. kern/mm/pmm.c：仅修改 init_pmm_manager() 中的管理器选择，使其指向 best_fit_pmm_manager。

操作要求：
- 必须修改项目中的实际文件，不能仅输出代码片段。
- 先阅读参考实现、相关头文件和两个现有测试。
- 完整保留 basic_check() 和 best_fit_check() 的签名及函数体。
- 不修改 default_pmm.c、公开头文件、构建文件及其他实验占位代码。
- 不修改物理内存探测、设备树解析、地址转换、Page 数组布局、页面保留和物理内存初始化逻辑。
- 不引入动态内存分配、外部依赖或针对测试序列的特殊分支。

输出要求：
- 使用以下上下文、接口清单和行为规格完成修改，以工程实际定义为准。
- 完成后报告修改文件、实现策略和实际验证结果。
- 如构建或运行被工具链、其他实验未完成代码阻碍，列出具体原因，不得将未执行的检查报告为通过。

[RELY]

参考文件

- kern/mm/default_pmm.c：参考页状态维护、空闲块管理、分割、合并及注册方式。
- kern/mm/best_fit_pmm.c：包含不可修改的两个测试函数。
- kern/mm/pmm.h：管理器接口、全局包装函数和地址转换。
- kern/mm/memlayout.h：页描述符、空闲区结构及页标志。
- kern/mm/best_fit_pmm.h：管理器外部声明。
- libs/list.h：双向循环链表接口。

已有数据结构

以下定义来自工程，不得重复定义：
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;

struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};
页状态与链表接口

可以使用工程已有的：

- PageReserved、SetPageReserved、ClearPageReserved
- PageProperty、SetPageProperty、ClearPageProperty
- set_page_ref、page_ref
- le2page
- list_init、list_empty
- list_next、list_prev
- list_add、list_add_before、list_add_after、list_del
- assert

其中：

- list_add(a, b) 将 b 插入 a 之后。
- 链表头不是 Page 节点，不得对其使用 le2page。
- 每个空闲块仅将首页的 page_link 加入链表。
- 空闲块首页的 property 是块内页数，不是字节数。
- PageProperty 表示空闲块首页，不是所有空闲页共有的标志。

全局 alloc_pages()、free_pages()、nr_free_pages() 会转调当前管理器回调，回调内部不得通过这些包装函数调用自身。

地址与资源语义

工程通过以下关系转换页描述符与物理地址：

page2ppn(page) = page - pages + nbase
page2pa(page)  = page2ppn(page) << PGSHIFT

必须遵守：

- struct Page * 是描述符地址，不是对应物理页的物理地址或内核虚拟地址。
- base + n 移动 n 个页描述符，对应向后移动 n 个物理页。
- 物理相邻性使用同一 Page 数组内的描述符关系判断，例如一个块的末尾是否等于另一个块的起点。
- 不将描述符指针直接强制转换为物理地址，不在描述符指针上额外乘以 PGSIZE。
- 上游负责物理范围裁剪、完整页面对齐、Page 数组建立和保留页划分。
- 本模块只管理上游通过 init_memmap() 登记的页面，不扩展可管理物理范围，不自行加入未登记页面。
- init_memmap() 接收的页面初始为保留状态，登记时将其转为空闲状态；未登记的内核、固件等占用页面仍不可分配。

设备树异常、物理范围截断、剩余空间不足一页的处理不属于本次修改范围，不得为处理这些情况修改上游初始化代码。
测试状态约定

现有测试会直接保存、清空并恢复 free_list 和 nr_free。

必须提供：

static free_area_t free_area;
#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

不得引入依赖额外保存和恢复的全局缓存、索引或统计状态。

测试可能在链表已清空、计数尚未清零时调用分配函数；此时必须返回 NULL，不得访问先前保存的链表。

[GUARANTEE]

必须实现：

static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);

必须定义：

const struct pmm_manager best_fit_pmm_manager;

注册对象必须将各回调绑定到对应函数，将 .check 绑定到已有 best_fit_check()，将 .name 设置为 "best_fit_pmm_manager"。

必须修改已有函数：

static void init_pmm_manager(void);

仅将管理器选择从 default_pmm_manager 切换为 best_fit_pmm_manager，保留输出和初始化调用。

不得修改：

static void basic_check(void);
static void best_fit_check(void);

允许添加模块内部的 static 辅助函数和必要的前置声明，不改变公开接口。

[SPECIFICATION]
共享状态约束

正常管理状态下：

- 空闲链表按块起始地址升序排列。
- 各块互不重叠，每个块仅有一个链表节点。
- 每个块的首页设置 PageProperty，且 property 等于块长。
- 块内非首页及已分配页均不设置 PageProperty。
- 所有空闲块长度之和等于 nr_free。
- 每次释放完成后，不存在仍可合并的相邻空闲块。

上述计数一致性不要求在测试主动清空链表、尚未调整计数的中间时刻成立，但该时刻的空链表分配必须安全失败。

best_fit_init
Pre-Condition：

- 管理器尚未登记可用页区间。

Post-Condition：

- list_empty(&free_list) 为真。
- nr_free == 0。

best_fit_init_memmap
Pre-Condition：

- 管理器已初始化。
- n > 0。
- [base, base + n) 是上游提供的有效描述符区间。
- 区间中每页均处于保留状态，且没有重复登记。

Post-Condition：

- 区间内每页不再保留，引用计数为 0。
- 区间被完整纳入空闲链表，不包含区间外页面。
- 空闲块首页标志及长度正确。
- 空闲页总数等于调用前计数加 n。
- 先前登记的区间仍可管理。

Requirements：

- 断言检查 n > 0 及各页的保留状态。
- 支持多次调用，不覆盖原有空闲链表。
- 可合并相邻登记区间；若合并，必须同步维护节点、首页标志和长度。

best_fit_alloc_pages

Pre-Condition：

- n > 0。
- 当前空闲链表有效。
Post-Condition：

- 成功时分配恰好 n 个物理连续页，返回最低地址页的描述符。
- 所分配页引用计数为 0，均不具有 PageProperty。
- 成功时空闲页总数减少 n。
- 失败时返回 NULL，链表、页状态及计数均不变。

Case 1：没有满足条件的块
链表为空，或所有块长度都小于 n 时，返回 NULL。总空闲页数足够但分散在多个不连续块中，也必须失败。

Case 2：选中块恰好为 n 页
分配整个块，移除其链表节点，清除其首页空闲块标志。

Case 3：选中块大于 n 页
分配该块低地址端的 n 页；剩余区间仍构成空闲块，其首页、长度和链表位置正确。

Requirements：

- 断言检查 n > 0。
- 从所有长度不少于 n 的块中选择长度最小者；同长度时选择起始地址较小者。
- 不能遇到第一个足够大的块就结束搜索，除非已经恰好匹配。
- 不要求特定时空复杂度。

best_fit_free_pages

Pre-Condition：

- n > 0。
- [base, base + n) 属于已登记的有效页面。
- 这些页面当前已分配，均未保留，且不属于任何空闲块。
- 区间可包含此前多次相邻分配得到的页面。

Post-Condition：

- 释放区间成为空闲页，引用计数为 0。
- 空闲页总数仅增加 n。
- 空闲链表仍按地址排列。
- 与释放区间直接相邻的空闲块全部合并。
- 最终块的首页标志及长度正确，被吸收块不再保留独立节点或首页标志。

Case 1：两侧均不相邻
释放区间成为独立块，链表节点数增加 1。

Case 2：仅一侧相邻
释放区间与该侧空闲块合并，链表节点总数不变。

Case 3：两侧均相邻
释放区间与前后两块合并，链表节点总数比调用前减少 1。

Requirements：

- 断言检查 n > 0，以及参考实现要求的非保留、非空闲块首页状态。
- 不要求建立分配记录或按单次分配边界释放。
- 无须为无效指针、越界释放或重复释放提供恢复机制；这些不属于合法调用。

best_fit_nr_free_pages

Pre-Condition：

- 管理器已初始化。

Post-Condition：

- 返回值等于当前 nr_free。
- 不改变任何管理状态。

best_fit_pmm_manager

Pre-Condition：

- 所有被引用函数已经定义或具有匹配的前置声明。

Post-Condition：

- 对象符合 best_fit_pmm.h 的外部声明。
- 六个回调均绑定正确。
- .check == best_fit_check。
- .name 为 "best_fit_pmm_manager"。

init_pmm_manager

Pre-Condition：

- 内核执行管理器选择流程。

Post-Condition：

- pmm_manager == &best_fit_pmm_manager。
- 原有名称输出仍执行。
- Best-Fit 的初始化回调被调用。
- 后续全局分配、释放、计数查询及检查进入 Best-Fit 回调。

验收与证据

逐项核对并报告：

1. 修改边界：通过最终差异确认，仅修改两个目标文件中的授权区域；两个测试函数保持原样。
2. 空状态：初始化后链表为空、计数为 0；空链表分配返回 NULL，包括计数暂时非零的测试状态。
3. Best-Fit 选择：原有测试中同时存在 2 页块和 1 页块时，申请 1 页返回后者，随后申请 2 页仍成功。
4. 失败不改变状态：无满足条件的连续块时，返回 NULL，链表、长度、标志和计数保持不变。
5. 分割：从长度为 m 的块申请 n 页且 m > n 时，剩余块从原首页加 n 开始，长度为 m - n，空闲计数减少 n。
6. 释放与合并：分别检查无邻居、仅前邻、仅后邻、两侧邻居四种情况的块长度、节点数量、首页标志和计数。
7. 测试恢复：原有测试结束后，空闲块数量和空闲页总数恢复初值。
8. 实际启用：检查 init_pmm_manager() 的赋值；运行时管理器名称为 best_fit_pmm_manager，检查回调为 best_fit_check()。
9. 资源范围：通过代码检查确认，所有可分配块仅来源于已登记区间及其分割、释放和合并，没有自行构造额外物理范围。

现有测试未覆盖的验收项，可通过代码路径检查或独立临时验证进行确认，不得修改原有两个测试。报告时区分运行验证和静态检查，不将静态检查描述为测试通过。


```

##### 3.buddy system（伙伴系统）分配算法

```
1.生成并接入 Buddy 分配模块的提示词
[PROMPT]

任务：在当前 Lab 2 工程中，从零实现 Buddy System 物理页分配器，并接入内核，使它替换当前使用的 Best-Fit 分配器。

操作要求：
- 先阅读 kern/mm/pmm.h、kern/mm/memlayout.h、kern/mm/default_pmm.c、kern/mm/best_fit_pmm.c、kern/mm/pmm.c 和 Makefile，再修改实际工程文件，不要只展示示例代码。
- 新建 kern/mm/buddy_pmm.c 和 buddy_pmm.h，自行设计内部状态，定义并注册 buddy_pmm_manager，实现 struct pmm_manager 的全部函数。
- 只修改 Buddy 模块及接入所必需的代码。不要改动 page_init()、Best-Fit 实现和课程已有测试。
- 在 kern/mm/pmm.c 的 init_pmm_manager() 中选择 &buddy_pmm_manager，确保内核运行和 check() 检查的都是 Buddy 实现。

算法要求：
1. 空闲块的页数必须是 2 的幂。init_memmap(base, n) 要正确处理起点不对齐、n 不是 2 的幂的连续空闲区，完整管理其中的页面。
2. alloc_pages(n) 找到能容纳 n 页的最小阶空闲块；必要时逐级拆分。没有可用块时返回 NULL。
3. free_pages(base, n) 根据本次分配实际占用的块大小归还页面，只与同阶且确实空闲的伙伴逐级合并。说明非 2 的幂请求如何分配和释放。
4. nr_free_pages() 返回实际空闲页数；分配、拆分、释放、合并后计数都要正确。
5. 正确维护 Page 的 flags、property、ref 和 page_link，不得将保留页加入空闲块。

[RELY]

以当前工程里的 struct pmm_manager、struct Page、PageReserved/PageProperty 宏、链表函数，以及 alloc_pages()/free_pages() 的真实定义为准。伙伴关系应根据物理页号和块大小判定，并说明所用的对齐规则。

[SPECIFICATION]

允许在 Buddy 模块内部增加静态辅助函数、静态变量和分阶空闲链表；不要修改 struct pmm_manager 的成员签名，不要新增供其他模块调用的接口。

init()
Pre-Condition：分配器尚未初始化。
Post-Condition：各阶空闲链表为空，空闲页总数为 0。
Requirements：初始化后可调用一次或多次 init_memmap() 注册空闲区。

init_memmap(base, n)
Pre-Condition：n > 0；[base, base+n) 是 page_init() 交付的连续保留页，且在 pages 数组范围内。
Post-Condition：这 n 页全部成为可管理的空闲页；空闲页总数恰好增加 n。
Requirements：按物理页号对齐要求，将该区间拆成互不重叠的 2 的幂次块；不能管理区间外的页面。

alloc_pages(n)
Pre-Condition：n > 0；空闲链表及空闲页总数已正确维护。
Post-Condition：
- 成功时返回一个块的起始 Page。该块实际占用 2^ceil(log2(n)) 页，至少包含 n 个连续页；空闲页总数减少实际占用页数。
- 失败时返回 NULL，所有链表、Page 状态和空闲页总数保持不变。
Requirements：优先寻找能够容纳请求的最小阶空闲块；如需使用更大块，则逐级对半拆分。记录该次分配的实际块大小，供 free_pages() 使用。

free_pages(base, n)
Pre-Condition：base 是一次成功分配返回的起始 Page，n 是对应的原始请求页数；该块尚未释放。
Post-Condition：该次分配实际占用的整个块重新空闲；空闲页总数增加实际块大小；能合并的同阶空闲伙伴被逐级合并。
Requirements：伙伴必须根据物理页号和当前块大小判断，不能仅凭地址相邻就合并；不得触及保留页或区间外页面。说明重复释放、错误参数在本实现中如何处理。

nr_free_pages()
Post-Condition：返回当前所有空闲块所含页数之和，调用本身不改变状态。

check()
Post-Condition：测试通过后，分配器状态及空闲页总数恢复到测试前；失败时通过断言暴露具体问题。
Requirements：覆盖分配取整、拆分、合并、分配失败和计数守恒。

[GUARANTEE]

实现须能在本工程的 C99、-Wall、-Werror 配置下编译；内核启动时打印 buddy_pmm_manager，且 Buddy 的 check() 通过。请给出修改的文件、算法关键约定、实际执行的命令与真实输出。无法运行时如实说明，不要报告未经验证的“通过”。
```

```
2.针对生成的实现编写测试函数的提示词
[PROMPT]

任务：阅读当前工程中第一份 prompt 实际生成的 Buddy 代码，为该实现设计并接入测试函数，验证分配、拆分、释放与合并是否正确。
操作要求：
- 先阅读 buddy_pmm.c/.h、pmm.h、memlayout.h 和 pmm.c，再编写测试；测试必须符合实际实现的状态约定。
- 在实际工程文件中加入测试，并让 Buddy 管理器的 check() 调用它。不要修改课程原有测试、page_init() 或 Best-Fit 模块。
- 每项测试使用断言检查结果，并在测试结束时确认空闲页总数恢复到测试前。

测试至少覆盖：
1. 初始化后的空闲块大小、地址对齐与空闲页计数。
2. 1 页、2 的幂次页和非 2 的幂页数（例如 3 页）的分配与释放。
3. 大块逐级拆分、同阶伙伴释放后逐级合并。
4. 相邻但不是伙伴的块不得错误合并。
5. 没有合适块时返回 NULL，失败后空闲页计数不变。
6. 空闲区起点不对齐、长度不是 2 的幂时，页面不遗漏、不越界。

[RELY]

测试中的预期地址和计数应根据当前 Buddy 实现及实际测试场景推导，不要假定任意两次分配一定得到相邻页面。若需要构造独立测试场景，必须保存并恢复原有分配器状态，不能破坏内核后续运行。

[GUARANTEE]

说明每项测试的预期结果、覆盖的行为及其验证理由；编译并运行测试，提供真实输出。发现失败时先定位并修复实现或测试，再报告最终结果。

请逐项根据 [SPECIFICATION] 的 Post-Condition 设计断言，特别验证失败操作不改变状态，以及测试结束后状态恢复。


```

##### 4.任意大小的内存单元 slub 分配算法

```
[PROMPT]

任务：
从零实现简化 SLUB 内存对象分配器，体现“两层分配”：
- 第一层复用现有 PMM，分配和释放连续物理页。
- 第二层在页内划分固定大小对象槽，按大小类分配任意字节数的对象。
- 超过最大小类的请求通过连续多页分配处理。

新增：
- kern/mm/slub.h：公开接口与使用约定。
- kern/mm/slub.c：分配器实现。

修改：
- kern/init/init.c：包含新头文件，在 pmm_init() 返回后调用 slub_init()。

操作要求：
- 必须修改项目实际文件，不仅展示代码。
- 先阅读 PMM、地址转换、链表和构建规则，再实现。
- 不修改 PMM 实现、管理器选择、Page 结构、设备树解析、地址映射及已有测试。
- 不补写 page_init() 等其他实验内容。
- 本次不实现测试函数，不调用尚不存在的测试接口。
- 仅实现单核、串行、非中断上下文使用的简化版本，不要求 Linux SLUB 的完整功能。
- 不引入标准库 malloc、外部依赖或递归依赖自身的元数据分配。

输出要求：
- 完成后说明数据布局、大小类、页回收策略及修改位置。
- 编译并在条件允许时启动内核，分别报告编译和运行结果。
- 若底层初始化缺失，报告阻塞原因，不删除初始化调用、不伪造实现或成功输出。

[RELY]

已有接口

kern/mm/pmm.h 已提供：

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);

extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;

static inline ppn_t page2ppn(struct Page *page);
static inline uintptr_t page2pa(struct Page *page);
static inline struct Page *pa2page(uintptr_t pa);

相关真实关系：

page2ppn(page) = page - pages + nbase;
page2pa(page) = page2ppn(page) << PGSHIFT;

PGSIZE 为 4096，来自 kern/mm/mmu.h。

npage 是物理页号的排他上界，不是可分配页数量；Page 数组下标对应物理页号减去 nbase。

PMM 分配失败返回 NULL，成功返回描述符指针，不直接返回可读写的内核地址。

地址转换约束

- struct Page * 是描述符地址，不能当作对象存储地址。
- 在现有直接映射内，物理页对应的内核地址为：
  page2pa(page) + va_pa_offset。
- 本工程的 KADDR 宏处于注释状态，不得假设可以直接调用。
- 若需要反向转换，应先验证地址属于受管理的直接映射范围，再转换，不能在检查前解引用任意地址。
- base + n 移动 n 个页描述符；对象内字节偏移应使用字节指针计算。
- 不扩展 PMM 登记的范围，不分配固件、内核、页表或描述符占用的保留页。

可依赖设施

使用项目已有的 size_t、uintptr_t、链表接口、assert、panic、cprintf 和内存操作函数。

工程使用 -nostdinc，不能假设标准 C 库头文件或 SIZE_MAX 等宏存在；以项目实际定义为准。

Makefile 已扫描 kern/mm 中的 C 文件，先确认新文件会被纳入构建，不无故修改构建规则。

kern/init/init.c 已在启动流程中调用 pmm_init()。

[GUARANTEE]

必须新增并实现：

void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);

在 slub.h 定义本模块的新约定：

#define SLUB_ALIGNMENT 16
#define SLUB_SMALL_MAX 2048

大小类固定为：
16、32、64、128、256、512、1024、2048 字节。

可以增加 static 辅助函数、内部结构和静态状态，不增加测试专用公开接口。

必须完成 kern_init() 中的初始化接入，且调用顺序为：
pmm_init() 完成后，调用 slub_init()。

[SPECIFICATION]

整体设计

Pre-Condition：

- PMM、页描述符及直接映射已经初始化。
- 分配器通过公开接口串行使用。

Post-Condition：

- 小对象来自按大小类组织的单页 slab。
- 一个小对象 slab 能容纳多个对象，而不是每个小对象独占一页。
- 管理元数据与对象有效载荷不重叠。
- 任意两个存活对象的请求字节区间不重叠。
- 全部对象释放后，本模块占用的动态物理页全部归还 PMM。

Requirements：

- 体现按大小类缓存、页内对象空闲链和对象占用计数。
- 为每个 slab 保存所属大小类、容量及存活数量等必要信息。
- 管理结构可以放在 slab 页内，大小类表可使用静态存储。
- 空闲槽可存放空闲链指针；对象分配后，调用方可覆盖完整请求字节数。
- 不修改 Page.property、PMM 页标志或 PMM 链表来保存 SLUB 私有状态。
- 通过模块自己的元数据确定对象归属和大对象的原始页数。
- 不要求特定时间或空间复杂度。

slub_init

Pre-Condition：

- PMM 已完成初始化。
- 本函数在启动时调用一次，尚无 SLUB 对象。

Post-Condition：

- 各大小类和内部链表初始化完成。
- 不向 PMM 申请物理页，初始化前后空闲页数相同。

slub_alloc

Pre-Condition：

- slub_init() 已执行。
- 调用发生在单核、串行、非中断上下文。

Post-Condition：

- 成功时返回 16 字节对齐的内核虚拟地址。
- 调用方能够读写从返回地址开始的完整 size 字节。
- 不改变其他存活对象的内容。
- 失败时返回 NULL，不遗留页、槽或元数据资源。
- 不保证返回内存清零。

Case 1：size 为 0
返回 NULL，不申请物理页，不修改已有对象状态。

Case 2：1 <= size <= SLUB_SMALL_MAX
选择能够容纳请求的最小大小类。优先使用该类尚有空闲槽的 slab；没有可用槽时，向 PMM 申请一页并建立新 slab。页内布局须计入元数据和对齐损耗，不得以 PGSIZE 除以对象大小直接忽略管理开销。

Case 3：size > SLUB_SMALL_MAX
申请足够的连续物理页，容纳请求、管理元数据和对齐空间。保存原始页描述符和页数，使释放时能够完整归还。

Case 4：资源不足
底层页分配失败时返回 NULL。已有对象、计数和链表保持有效；如失败前取得了临时资源，必须回收。

Case 5：算术溢出或请求超出管理范围
在加法、对齐、乘法或页数计算溢出前拒绝请求，返回 NULL。不得将超大请求回绕成小分配。

slub_free

Pre-Condition：

- 参数为 NULL，或本模块返回且尚未释放的对象起始地址。
- 不要求处理重复释放、对象内部指针、栈地址或其他非法指针。

Post-Condition：

- 对象不再存活，其他存活对象内容不变。
- 小对象槽能够再次分配。
- 空 slab 立即归还 PMM，不保留空 slab 缓存。
- 大对象对应的全部原始页一次性归还 PMM。
- 归还页面后不再访问其内部元数据。

Case 1：ptr 为 NULL
无操作，空闲页数及分配器状态不变。

Case 2：释放后 slab 仍有存活对象
仅回收该槽，必要时将原满 slab 转为可分配状态，不释放整页。

Case 3：释放 slab 的最后一个对象
先从内部管理结构移除 slab，再归还整页；后续不得保留指向已归还页的链表节点。

Case 4：释放大对象
使用其记录的原始描述符和页数释放，不根据用户指针所在的单页猜测分配长度。

接入与验证边界

Pre-Condition：

- 底层 PMM 能正常初始化和分配页面。

Post-Condition：

- 新模块参与编译和链接。
- 启动流程在 PMM 完成后初始化 SLUB。
- 原有 PMM 检查仍保留。

Requirements：

- 检查最终差异，确认只新增两个模块文件并修改初始化接入。
- 当前若仍缺少 page_init()，明确报告该依赖阻止完整构建，不补写、不绕过。
- 不将“源文件编译通过”描述为“内核运行通过”。

```

```
[PROMPT]

任务：
阅读第一份提示词生成的真实 SLUB 实现，为其设计并实现独立测试，检验不同大小对象的分配、读写、释放、复用和资源回收。

新增：

- kern/mm/slub_test.h
- kern/mm/slub_test.c

修改：

- kern/init/init.c：包含测试头文件，在 slub_init() 之后调用 slub_check()。

操作要求：

- 必须修改实际文件。
- 先读取 slub.h、slub.c、PMM 接口和初始化流程。
- 不修改 SLUB 实现、PMM 实现、原有测试、地址转换或构建规则。
- 发现分配器缺陷时报告触发用例和断言，不通过修改测试期望掩盖缺陷。
- 测试只通过公开接口操作分配器，不访问私有链表，不假设空闲槽的返回顺序。
- 测试中使用静态数组保存指针和记录，避免用被测分配器分配测试管理数据。
- 不补写 page_init() 等其他实验内容。

输出要求：

- 实现并接入测试，给出“测试条件—断言—可发现缺陷”的对应关系。
- 使用项目实际构建方式验证；分别报告构建、启动和测试结果。
- 测试未执行或被上游依赖阻塞时，明确说明，不输出或宣称测试通过。

[RELY]

第一份实现约定的公开接口：

void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);

#define SLUB_ALIGNMENT 16
#define SLUB_SMALL_MAX 2048

大小类为：
16、32、64、128、256、512、1024、2048 字节。

可依赖 PMM 接口：

size_t nr_free_pages(void);

以及：

- 项目实际的 PGSIZE，当前为 4096。
- assert、cprintf 及项目内存操作函数。
- size_t、uintptr_t 等工程类型。

分配器承诺：

- 零长度请求返回 NULL。
- 返回指针按 16 字节对齐。
- 每个成功请求至少可读写请求字节数。
- slub_free(NULL) 无操作。
- 空 slab 立即回收，大对象全部释放原始页。
- 全部对象释放后，空闲页数恢复测试前值。
- 无法满足或算术溢出的请求返回 NULL，不泄漏资源。
- 不保证清零，不保证释放后返回相同地址。
- 非法指针释放、重复释放、并发和中断调用不在接口契约内。

[GUARANTEE]

必须新增并实现：

void slub_check(void);

允许新增 static 测试辅助函数、静态指针数组和记录结构。

必须修改 kern_init() 的接入部分，使：

- slub_check() 在 slub_init() 后调用。
- 测试成功标志仅在全部断言执行完毕后输出。

不修改被测实现及已有 PMM 检查。

[SPECIFICATION]

slub_check

Pre-Condition：

- PMM 和 SLUB 已初始化。
- 没有其他 SLUB 使用者或并发分配操作。
- 至少有 16 个空闲物理页；不足时明确报错，不输出成功。
- 测试数组容量足够，记录数量不越界。

Post-Condition：

- 下列测试全部执行。
- 所有成功分配的对象均恰好释放一次。
- 最终空闲页数与入口基准相等。
- 全部断言通过后输出：
  slub_check() succeeded!

Case 1：零长度与空指针释放
记录空闲页数，断言 slub_alloc(0) == NULL，执行 slub_free(NULL)，断言空闲页数未变。

Case 2：大小类和页面边界
覆盖：

- 1；
- 每个大小类 C 的 C-1、C、C+1；
- PGSIZE-1、PGSIZE、PGSIZE+1；
- 2*PGSIZE+37。

可以去重。逐个分配，检查非 NULL、16 字节对齐，并写入及核对完整请求区间；释放后断言空闲页数恢复该用例前值。

Case 3：多对象同时存活与内容隔离
同时分配至少 64 个混合大小对象，包含小对象和多页对象。为不同对象写入可区分的字节模式，并检查所有存活对象的完整内容。使用经过溢出检查的地址区间比较，断言任意两个请求区间不重叠。禁止写入请求范围外的大小类尾部空间。

Case 4：验证页内复用
先分配一个 16 字节对象，记录此时空闲页数，再分配另一个 16 字节对象。断言第二次不额外消耗物理页，且两个对象不重叠。释放后空闲页数恢复基准。该用例用于排除“每个小对象独占一页”的实现。

Case 5：跨 slab、满 slab 和部分释放
连续分配 PGSIZE / 64 + 1 个 64 字节对象，确保跨越单页容量；检查至少消耗两页。全部写入模式后，交错释放其中一部分，复查剩余对象内容，再申请相同数量的 64 字节对象并检查其可写性与不重叠性。只在实际实现可证明存在足够同类空槽时，断言该补充分配阶段不申请新页。不得断言返回指针必须与已释放指针逐个相等。

Case 6：释放顺序与最后对象回收
分别以正序、逆序和固定的交错顺序释放一组对象，覆盖一个 slab 中部分对象仍存活和最后一个对象被释放的路径。每轮结束后断言页数恢复基准，重复至少 10 轮，检查是否有累计泄漏或悬空链表问题。

Case 7：大对象完整读写与回收
对多页对象写入并验证整个请求区间，尤其覆盖跨页位置和最后一个字节。同时保留其他对象，释放大对象后复查其他对象内容。全部释放后检查空闲页数恢复，捕获只释放首页或记录页数错误的问题。

Case 8：超大请求和失败原子性
保持至少一个已经写入模式的小对象存活，记录空闲页数：

- 申请 (size_t)-1，检查返回 NULL。
- 根据当前空闲页数构造大于全部可用页容量的请求；先检查测试本身的乘法、加法不会溢出，再执行。

两次失败后都断言空闲页数不变、存活对象内容不变，并确认后续正常分配仍可成功。最后释放对象并恢复基准。

Requirements：

- 断言必须检查实际返回值、地址、内容或页计数，不能仅打印说明。
- 每个失败断言应能通过测试阶段输出定位到具体用例。
- 不读写已经释放的对象，不通过非法释放制造内核崩溃。
- 不要求未约定的行为：内存清零、固定返回地址、固定链表顺序或固定大对象元数据大小。
- 用例数量和内存需求必须有明确上限，不无限申请至内核耗尽。
- 初次调用测试后再完整调用一次，确认可重复执行且无残留状态。
- 如静态检查或测试发现实现缺陷，保留有效断言并报告，不修改被测模块。

测试接入

Pre-Condition：

- 第一份任务已完成初始化接入。
- 底层物理内存初始化可正常完成。

Post-Condition：

- 测试文件参与编译和链接。
- 原有 PMM 测试正常执行，随后执行 SLUB 初始化与测试。
- 只有全部检查完成才输出成功标志。

Requirements：

- 当前工程若仍缺少 page_init()，报告上游依赖阻塞，不删除该调用。
- 内核停留在固件输出、未进入测试或没有成功标志，均不能判定测试通过。




```


