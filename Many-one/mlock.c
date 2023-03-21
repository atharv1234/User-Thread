#include "mlock.h"
#include "func.h"
void mutex_unlk(q_tcb *x, mutxlk *lk_var)
{
    node_queue *tempNode = x->front;
    thr_cntrl_blk **to_re_queue = NULL;
    int num_of_requeu = 0;
    while (tempNode)
    {
        if (tempNode->thr_ctr_blk_node->wait_mutex == lk_var)
        {
            tempNode->thr_ctr_blk_node->wait_mutex = NULL;
            tempNode->thr_ctr_blk_node->thr_state = RUNNABLE;
            to_re_queue = (thr_cntrl_blk **)realloc(to_re_queue, ++num_of_requeu);
            to_re_queue[num_of_requeu - 1] = tempNode->thr_ctr_blk_node;
            break;
        }
        tempNode = tempNode->next;
    }
    for (int i = 0; i < num_of_requeu; i++)
    {
        re_queue(x, to_re_queue[i]);
    }
    free(to_re_queue);
    return;
}
int initialize_mutex(mutxlk *pass_lock)
{
    volatile int *locking_vari = &(pass_lock->lk);
    int o_val;
    timer_off();
    asm(
        "movl $0x0,(%1);"
        : "=a"(o_val)
        : "a"(locking_vari));
    pass_lock->lker = 0;
    timer_on();
    return 0;
}
int acq_mtx(mutxlk *pass_lock)
{
    volatile int o_val;
    timer_off();
    volatile int *locking_vari = &(pass_lock->lk);
    asm(
        "xchg   %%al,(%1);"
        "test   %%al, %%al;"
        "je end;"
        : "=a"(o_val)
        : "a"(locking_vari)
        : "%ebx");
    running_proc->wait_mutex = pass_lock;
    running_proc->thr_state = WAITING;
    swtchTosched();
    asm(
        "end:");
    pass_lock->lker = running_proc->tid;
    timer_on();
}
int relse_mtx(mutxlk *pass_lock)
{
    timer_off();
    if (pass_lock->lker != running_proc->tid)
    {
        timer_on();
        return 131;
    }
    volatile int *locking_vari = &(pass_lock->lk);
    int o_val;
    asm(
        "movl $0x0,(%1);"
        : "=a"(o_val)
        : "a"(locking_vari));
    mutex_unlk(&thread_set, pass_lock);
    pass_lock->lker = 0;
    timer_on();
}
