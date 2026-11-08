#include <clock.h>      //时钟/定时器相关
#include <console.h>    //控制台输出
#include <defs.h>       //通用定义，包含一些基础类型以及宏定义
#include <intr.h>       //中断开关
#include <kdebug.h>     //内核调试工具
#include <kmonitor.h>   //内核监视器
#include <pmm.h>        //物理内存管理
#include <stdio.h>      //标准库的memset等
#include <string.h>     
#include <trap.h>       //Trap处理相关，idt_init()在这里声明
//设备树解析，dtb_init()
#include <dtb.h>
//CPU上电后，第一段引导代码做完最底层的事情，就会跳转到init.c
int kern_init(void) __attribute__((noreturn));    //函数声明，attribute是GCC的扩展，表明这个函数永远不会返回
void grade_backtrace(void);

int kern_init(void) {
    extern char edata[], end[];   //edata--数据段的结束地址，end是整个内核镜像的结束地址
    // 先清零 BSS，再读取并保存 DTB（设备树二进制对象） 的内存信息，避免被清零覆盖（为了解释变化 正式上传时我觉得应该删去这句话）
    memset(edata, 0, end - edata);//把edata-end之间的全部地址写成0
    dtb_init();
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);

    print_kerninfo();           //打印内核自身的体检报告

    // grade_backtrace();
    idt_init();  // init interrupt descriptor table --把Trap处理函数_alltraps的地址写进stvec寄存器，并做相关准备

    pmm_init();  // init physical memory management

    idt_init();  // init interrupt descriptor table

    clock_init();   // init clock interrupt--配置定时器，让程序周期性触发中断
    intr_enable();  // enable irq interrupt--打开全局中断开关，CPU开始接收中断

    /* do nothing */
    while (1)
        ;
}
//_attribute_((noinline))禁止编译器内联，栈回溯需要真实的函数调用栈帧，内敛后翰苏在栈上不存在，无法回溯演示
void __attribute__((noinline))
grade_backtrace2(int arg0, int arg1, int arg2, int arg3) {
    mon_backtrace(0, NULL, NULL);
}

void __attribute__((noinline)) grade_backtrace1(int arg0, int arg1) {
    grade_backtrace2(arg0, (uintptr_t)&arg0, arg1, (uintptr_t)&arg1);
}

void __attribute__((noinline)) grade_backtrace0(int arg0, int arg1, int arg2) {
    grade_backtrace1(arg0, arg2);
}

void grade_backtrace(void) { grade_backtrace0(0, (uintptr_t)kern_init, 0xffff0000); }

