#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void  //切换线程状态，将其变为运行态，让线程具备运行资格
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2512973
void
schedule(void)   //调度函数
{
    bool intr_flag;    //保存进入调度器前的中断使能状态
    local_intr_save(intr_flag);//保存原中断状态，并关闭本地中断
    {
        current->need_resched = 0;
        list_entry_t *last = (current == idleproc) ?
                            &proc_list : &current->list_link;//last 指向的是链表节点，不是线程控制块
        list_entry_t *le = last;
        struct proc_struct *next = idleproc;

        /* Include the current thread last, and never interpret the sentinel. */
        do
        {
            le = list_next(le);
            if (le != &proc_list)
            {
                struct proc_struct *proc = le2proc(le, list_link);
                //线程控制块地址 = list_link 地址 − list_link 在控制块内的偏移
                if (proc->state == PROC_RUNNABLE)
                {
                    next = proc;
                    break;
                }
            }
        } while (le != last);

        next->runs++;
        if (next != current)
        {
            proc_run(next);
        }
        //当目标不同，调用 proc_run() 完成交接：
        //保存旧线程指针。
        //更新 current。
        //安装目标页表并刷新 TLB。
        //调用 switch_to() 保存旧上下文、恢复目标上下文
    }
    local_intr_restore(intr_flag);
}


