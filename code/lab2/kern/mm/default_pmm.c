#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

static free_area_t free_area; //整个管理器的状态，本管理器唯一的全局状态对象

#define free_list (free_area.free_list) //一个双向循环链表头
#define nr_free (free_area.nr_free) //当前所有空闲页的总数

static void
default_init(void) {  //初始化管理器，在管理器被选用时调用一次，负责把全局状态置为"空"
    list_init(&free_list);//把链表头初始化为空的双向循环链表
    nr_free = 0;
}

static void
default_init_memmap(struct Page *base, size_t n) { //把一段物理页初始化为空间
    assert(n > 0);                                  //参数 base 是这段物理内存第一页对应的 struct Page 描述符指针；
    struct Page *p = base;                          //n 是页数
    for (; p != base + n; p ++) {
        assert(PageReserved(p));                     //断言当前页处于保留态，进来时它们必须还是 Reserved，否则说明重复登记。
        p->flags = p->property = 0;
        set_page_ref(p, 0);                          //把空闲页的引用计数设置为0
    }
    base->property = n;
    SetPageProperty(base);                           //在首页上设置 PageProperty 标志位
    nr_free += n;
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {                                         //链表非空，按物理地址升序插入，保持链表有序
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);//利用le2反推page宏反推出链表节点所属的struct page指针
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
}

static struct Page *
default_alloc_pages(size_t n) {                         //分配n个连续物理页,返回起始页描述符指针；失败则返回NULL
    assert(n > 0);                                      //页数必须为正          
    if (n > nr_free) {                                  //nc_free总的空闲页数
        return NULL;
    }
    struct Page *page = NULL;                           //候选块指针
    list_entry_t *le = &free_list;                      
    while ((le = list_next(le)) != &free_list) {        //le2page作用：ucore里的一个容器宏，已知一个链表节点的地址，反推出包含这个节点的整个struct page的地址
        struct Page *p = le2page(le, page_link);        //从链表节点还原出块首的struct Page指针
        if (p->property >= n) {
            page = p;                                   //记录下第一块足够大的核心并把它记录下来
            break;
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));//记录候选块的前驱节点，方便后续删除操作
        list_del(&(page->page_link));                      //候选块从空闲链表被删除，该块内存暂时脱离管理
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);                            //清除返回快首页的pagepropert标志，否则会释放时断言！pageproperty失败
    }
    return page;
}

static void
default_free_pages(struct Page *base, size_t n) {  //释放从base开始的n个连续页，需保证这n页是之前分配出去的
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {                       //保留页：不纳入物理内存管理器分配范围的物理页，普通的页表分配永远无法返回它
        assert(!PageReserved(p) && !PageProperty(p));//断言当前页不是保留页，也不是空闲块首
        p->flags = 0;                                //falgs位图是承载这类状态标记的位级数据结构
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t* le = list_prev(&(base->page_link)); //与前邻块合并
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;                               //把 base 指针更新为合并后的前块首
        }
    }

    le = list_next(&(base->page_link));             //与后邻块合并
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}                                                   //函数结束，释放的块已经尽可能与前后邻块合并成更大的空闲块，抑制外部碎片

static size_t
default_nr_free_pages(void) { //查询当前空闲页表总数
    return nr_free;
}

 //自检测试
static void
basic_check(void) {                                //一组单元测试，验证分配/释放的基本不变量
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);       //断言分配器没有重复分配同一页
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);
    //本管理器的分配不增加引用计数，由上层决定何时 set_page_ref
    assert(page2pa(p0) < npage * PGSIZE);           //断言物理地址在合法范围内
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;       //把空闲链表头按值拷贝到局部变量free_list_store,后续要破坏性修改链表做测试，测完再修复
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);                         //三次释放后空闲页表数应为3

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;                   //恢复原样
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};
