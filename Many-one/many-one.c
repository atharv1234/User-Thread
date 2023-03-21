#define _GNU_SOURCE
#define DEV
#include <stdio.h>
#include <sched.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <limits.h>
#include <stdlib.h>
#include <setjmp.h>
#include "mlock.h"
#include "func.h"
#include <unistd.h>
thr_cntrl_blk *running_proc=NULL;
thr_cntrl_blk *sched=NULL;
q_tcb thread_set;
thr_cntrl_blk *m_proc = NULL;
unsigned long int pidOfnext;
sched_parameters defined = {st : 0, mx : 100};
addr address_translation(addr address)
{
    addr return_val;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
                 : "=g"(return_val)
                 : "0"(address));
    return return_val;
}
void make_cntxt(sigjmp_buf *cntxt, void *functions, void *stk)
{
    sigsetjmp(*cntxt, 1);
    cntxt[0]->__jmpbuf[JB_BP] = address_translation((addr)(stk - sizeof(int)));
    cntxt[0]->__jmpbuf[JB_SP] = cntxt[0]->__jmpbuf[JB_BP];
    if (functions)
    {
        cntxt[0]->__jmpbuf[JB_PC] = address_translation((addr)functions);
    }
}
//if running then stop and add to list of queue(queue the running process)
void runningProcessQueue(q_tcb *x)
{
    node_queue *tempNode = x->front;
    if (tempNode == NULL)
    {
        return;
    }
    if (tempNode->thr_ctr_blk_node->thr_state == RUNNING)
    {
         thr_cntrl_blk *thrCtrblk_retur = tempNode->thr_ctr_blk_node;
        x->front = x->front->next;
        thrCtrblk_retur->thr_state = RUNNABLE;
        add_thread_to_queue(x, thrCtrblk_retur);
        free(tempNode);
        return;
    }
    while (tempNode->next)
    {
         node_queue *deleted_node_ = tempNode->next;
        if (deleted_node_->thr_ctr_blk_node->thr_state == RUNNING)
        {
            tempNode->next = deleted_node_->next;
             thr_cntrl_blk *thrCtrblk_retur = deleted_node_->thr_ctr_blk_node;
            if (deleted_node_ == x->back)
            {
                x->back = tempNode;
            }
            thrCtrblk_retur->thr_state = RUNNABLE;
            add_thread_to_queue(x, thrCtrblk_retur);
            free(deleted_node_);
            break;
        }
        tempNode = tempNode->next;
    }
    return;
}
//get the next runnable threa
thr_cntrl_blk* next_thread(q_tcb *tq)
{
    node_queue *tempNode = tq->front;
    if (tempNode == NULL)
    {
        return NULL;
    }
    if (tempNode->next == NULL)
    {
        if (tempNode->thr_ctr_blk_node->thr_state == RUNNABLE)
        {
            return tempNode->thr_ctr_blk_node;
        }
        else
        {
            return NULL;
        }
    }
    if (tempNode->thr_ctr_blk_node->thr_state == RUNNABLE)
    {
         thr_cntrl_blk *to_return_tcb = tempNode->thr_ctr_blk_node;
        tq->front = tq->front->next;
        add_thread_to_queue(tq, to_return_tcb);
        free(tempNode);
        return to_return_tcb;
    }
    while (tempNode->next)
    {
         node_queue *delNode_ = tempNode->next;
        if (delNode_->thr_ctr_blk_node->thr_state == RUNNABLE)
        {
            tempNode->next = delNode_->next;//detach the block
             thr_cntrl_blk *to_return_tcb = delNode_->thr_ctr_blk_node;
            if (delNode_ == tq->back)
            {
                tq->back = tempNode;
            }
            add_thread_to_queue(tq, to_return_tcb);
            free(delNode_);
            return to_return_tcb;
        }
        tempNode = tempNode->next;
    }
    return NULL;
}
void exited_threads(q_tcb *x)
{
    thread *deleted_thread = NULL;
    int numberOfdeleted = 0;
    node_queue *tempNode_ = x->front;
    while (tempNode_)
    {
        if (tempNode_->thr_ctr_blk_node->ext_process == 1)
        {
            deleted_thread = (thread *)realloc(deleted_thread, ++numberOfdeleted * sizeof(thread));
            deleted_thread[numberOfdeleted - 1] = tempNode_->thr_ctr_blk_node->tid;
        }
        tempNode_ = tempNode_->next;
    }
    for (int i = 0; i < numberOfdeleted; i++)
    {
        rem_thread(x, deleted_thread[i]);
    }
    free(deleted_thread);
    return;
}
static void manyOneSched()
{
    timer_off();
    runningProcessQueue(&thread_set);
    int is_exit = running_proc->ext_process;
    exited_threads(&thread_set);
    thr_cntrl_blk *nxt_Thread = next_thread(&thread_set);
    if (!nxt_Thread)
        return;

    if (is_exit == 0)
    {
        thr_cntrl_blk *__prev = running_proc;
        running_proc = nxt_Thread;
        nxt_Thread->thr_state= RUNNING;
        siglongjmp(*(nxt_Thread->contxt),1);
    }
    else
    {
        running_proc = nxt_Thread;
        nxt_Thread->thr_state = RUNNING;
        siglongjmp(*(nxt_Thread->contxt),1);
    }
}
void initialize_tcb(thr_cntrl_blk *tcb_block, int initial_state, thread thr_id, sigjmp_buf *cntxt)
{
    tcb_block->thr_state = initial_state;
    tcb_block->tid = thr_id;
    tcb_block->ext_process = 0;
    tcb_block->wait_process = NULL;
    tcb_block->numOfwait = 0;
    tcb_block->num_unaddressed_sig = 0;
    tcb_block->remaining_signal = NULL;
    tcb_block->wait_mutex = NULL;
    tcb_block->contxt = cntxt;
    tcb_block->stkPtr = NULL;
}
static void strt_timer(sched_parameters *time_span)
{
     struct itimerval val_var;
    val_var.it_interval.tv_sec = time_span->st;
    val_var.it_interval.tv_usec = time_span->mx;
    val_var.it_value.tv_sec = time_span->st;
    val_var.it_value.tv_usec = time_span->mx;
    if (setitimer(ITIMER_VIRTUAL, &val_var, NULL) == -1)
    {
        perror("set time");
        exit(1);
    }
    return;
}
static void *stack_allocation(size_t stk_size,size_t protect_area)
{
    void *stack = NULL;
    // For a memory region to be used as a stack, the MAP_STACK
    // flag has to be passed to mmap; otherwise, upon syscalls and pagefaults, if the stack-pointer 
    // register doesn’t point to such a page, the program gets killed.
    stack = mmap(NULL, stk_size , PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED)
    {
        perror("Failed stack allocation");
        return NULL;
    }
    if (mprotect(stack, protect_area, PROT_NONE))
    {
        munmap(stack, stk_size + protect_area);
        return NULL;
    }
    return stack;
}//get particular tid node address from the queue
static void initializeManyone(sched_parameters duration)
{
    printf("Many one started\n");
    thread_set.back = NULL;
    thread_set.front = NULL;
    thread_set.length = 0;

    m_proc = (thr_cntrl_blk *)malloc(sizeof(thr_cntrl_blk));
    sigjmp_buf *conText = (sigjmp_buf *)malloc(sizeof(sigjmp_buf));
    make_cntxt(conText, NULL, NULL);
    initialize_tcb(m_proc, RUNNING, getpid(), conText);
    running_proc = m_proc;

    pidOfnext = getpid() + 1;
    add_thread_to_queue(&thread_set, m_proc);

    sched = (thr_cntrl_blk *)malloc(sizeof(thr_cntrl_blk));
    conText = (sigjmp_buf *)malloc(sizeof(sigjmp_buf));
    // sched context
    void *sch_stk = stack_allocation(STK_SZ, 0) + STK_SZ;
    make_cntxt(conText, manyOneSched, sch_stk);
    initialize_tcb(sched, RUNNING, 0, conText);
    timer_on(&duration);
}
void wrap()
{
    fun_arg *tempArgNd = running_proc->argmnt;
    timer_on();
    (tempArgNd->f)(tempArgNd->argmnt);
    timer_off();
    running_proc->ext_process = 1;
    free(tempArgNd);
    swtchTosched();
}
int create_thr(thread *t, void *attributes, void *function, void *arGmnt)
{
    if (t == NULL || function == NULL)
    {
        return EINVAL;
    }
    timer_off();
    static int isInitialised = 0;
    if (!isInitialised)
    {
        if (attributes)
        {
            int mx = ((thread_attributes *)attributes)->schd_duration.mx;
            int st = ((thread_attributes *)attributes)->schd_duration.st;
            if (mx != 0 || st != 0)
            {
                initializeManyone(((thread_attributes *)attributes)->schd_duration);
            }
        }
        else
        {
            initializeManyone(defined);
        }
        isInitialised = 1;
    }
    sigjmp_buf *conText = (sigjmp_buf *)malloc(sizeof(sigjmp_buf));
    thr_cntrl_blk *tempBlk = (thr_cntrl_blk *)malloc(sizeof(thr_cntrl_blk));

    fun_arg *Farg = (fun_arg *)malloc(sizeof(fun_arg));
    Farg->f = function;
    Farg->argmnt = arGmnt;

    tempBlk->argmnt = Farg;
    if (attributes)
    {
        if (((thread_attributes *)attributes)->stack)
        {
            conText[0]->__jmpbuf[JB_SP] = address_translation((addr)((thread_attributes *)attributes)->stack + ((thread_attributes *)attributes)->stack_size);
            make_cntxt(conText, wrap, ((thread_attributes *)attributes)->stack + ((thread_attributes *)attributes)->stack_size);
        }
        else if (((thread_attributes *)attributes)->stack_size)
        {
            tempBlk->stkPtr = stack_allocation(((thread_attributes *)attributes)->stack_size, 0);
            make_cntxt(conText, wrap, tempBlk->stkPtr + ((thread_attributes *)attributes)->stack_size);
        }
    }
    else
    {
        tempBlk->stkPtr = stack_allocation(STK_SZ, 0) + STK_SZ;
        make_cntxt(conText, wrap, tempBlk->stkPtr);
    }

    initialize_tcb(tempBlk, RUNNABLE, pidOfnext++, conText);
    add_thread_to_queue(&thread_set, tempBlk);
    *t = tempBlk->tid;
    sigsetjmp(*m_proc->contxt, 1);
    timer_on();
    return 0;
}
int join_thr(thread t, void **returnLoc)
{
    timer_off();
    thr_cntrl_blk *wait_thr = get_thread_from_queue(&thread_set, t);
    if (wait_thr == NULL)
    {
        if (returnLoc)
            *returnLoc = (void *)ESRCH;
        timer_on();
        return ESRCH;
    }
    if (wait_thr->ext_process)
    {
        if (returnLoc)
            *returnLoc = (void *)EINVAL;
        timer_on();
        return EINVAL;
    }
    wait_thr->wait_process = (int *)realloc(wait_thr->wait_process, (++(wait_thr->numOfwait)) * sizeof(int));
    wait_thr->wait_process[wait_thr->numOfwait - 1] = running_proc->tid;
    running_proc->thr_state = WAITING;
    swtchTosched();
    return 0;
}
int kill_thread(pid_t x, int sig_number)
{
    if (sig_number <= 0)
    {
        return -1;
    }
    int ret_Val = 0;
    timer_off();
    if (sig_number == SIGINT || sig_number == SIGCONT || sig_number == SIGSTOP)
    {
        kill(getpid(), sig_number);
    }
    else
    {
        if (running_proc->tid == x)
        {
            raise(sig_number);
            timer_on();
            return ret_Val;
        }
        thr_cntrl_blk *tmpNode = get_thread_from_queue(&thread_set, x);
        //add to unaddressed signals list
        if (tmpNode)
        {
            tmpNode->remaining_signal = (int *)realloc(tmpNode->remaining_signal, (++(tmpNode->num_unaddressed_sig) * sizeof(int)));
            tmpNode->remaining_signal[tmpNode->num_unaddressed_sig - 1] = sig_number;
        }
        else
        {
            ret_Val = -1;
        }
    }
    timer_on();
    return ret_Val;
}
int exit_thread(void *return_Val)
{
    timer_off();
    running_proc->ext_process = 1;
    swtchTosched();
}
mutxlk testlock;
void func()
{
    printf("inside function\n");
    exit_thread(NULL);
    return;
}
void testfun1()
{
    acq_mtx(&testlock);
    printf("first thread acquiring lock and sleep\n");
    sleep(1);
    relse_mtx(&testlock);
}
void testfun2()
{
    printf("second thread acquiring the acquired lock\n");
    int ret = relse_mtx(&testlock);
    if (ret == 131)
    {
        printf("Test passed \n");
    }
    else
    {
        printf("Test failed \n");
    }
    printf("\n");
}
int main()
{
    thread t;
    printf("Create thread test :\n");
    if (create_thr(NULL, NULL, NULL, NULL) == EINVAL && create_thr(&t, NULL, NULL, NULL) == EINVAL && create_thr(NULL, NULL, func, NULL) == EINVAL)
    {
        printf("Test passed \n");
    }
    else
    {
        printf("Test failed \n");
    }
    printf("\n");
    printf("Join test with wrong arguements:\n");
    create_thr(&t, NULL, func, NULL);
    //thread already join
    join_thr(t, NULL);
    // irregular tid and join above again
    if (join_thr(t, NULL) == ESRCH && join_thr(231, NULL) == ESRCH)
    {
        printf("Test passed \n");
    }
    else
    {
        printf("Test failed \n");
    }
    printf("\n");
    printf("Send inappropriate signals\n");
    if (kill_thread(t, 0) == -1)
        printf("Test passed \n");
    else
        printf("Test failed \n");
    printf("\n");
    printf("Release without acquiring:\n");
    thread t1, t2;
    initialize_mutex(&testlock);
    create_thr(&t1, NULL, testfun1, NULL);
    create_thr(&t2, NULL, testfun2, NULL);
    join_thr(t1, NULL);
    join_thr(t2, NULL);
}
