#include "mlock.h"
#include <unistd.h>
#include "func.h"

thr_cntrl_blk *get_thread_from_queue(q_tcb *x, thread thr_id)
{
    node_queue *tempNode = x->front;
    while (tempNode != NULL)
    {
        if (tempNode->thr_ctr_blk_node->tid == thr_id)
        {
            return tempNode->thr_ctr_blk_node;
        }
        tempNode = tempNode->next;
    }
    return NULL;
}
//add thread to the queue
int add_thread_to_queue(q_tcb *x, thr_cntrl_blk *thr_tcb)
{
    node_queue *temp_node = (node_queue *)malloc(sizeof(node_queue));
    if (!temp_node)
        return -1;
    temp_node->thr_ctr_blk_node = thr_tcb;
    temp_node->next = NULL;

    if (x->front == NULL)
    {
        x->front = temp_node;
        x->back = temp_node;
        x->length++;
        return 0;
    }
    x->back->next = temp_node;
    x->back = temp_node;
    x->length++;
    return 0;
}
//free the initial memory and add to the end of the queue 
void re_queue(q_tcb *x, thr_cntrl_blk *thr_tcb)
{
    node_queue *tempNode = x->front;
    if (tempNode == NULL)
    {
        return;
    }
    if (tempNode->thr_ctr_blk_node->tid == thr_tcb->tid)
    {
        x->front = x->front->next;
        add_thread_to_queue(x, thr_tcb);
        free(tempNode);
        return;
    }
    while (tempNode->next)
    {
        node_queue *deletedNode_ = tempNode->next;
        if (deletedNode_->thr_ctr_blk_node->tid == thr_tcb->tid)
        {
            tempNode->next = deletedNode_->next;
            if (deletedNode_ == x->back)
            {
                x->back = tempNode;
            }
            add_thread_to_queue(x, thr_tcb);
            free(deletedNode_);
            break;
        }
        tempNode = tempNode->next;
    }
    return;
}
//called from exiting thread and frees the memory and requeues after setting to runnable
int rem_thread(q_tcb *x, unsigned long int thr_id)
{
    thread *list_of_requed = NULL;
    int num_of_requed = 0;
    node_queue *tempNode = x->front;
    if (x->front == NULL)
    {
        return 0;
    }
    if (x->front->thr_ctr_blk_node->tid == thr_id)
    {
        for (int j = 0; j < x->front->thr_ctr_blk_node->numOfwait; j++)
        {
            list_of_requed = (thread *)realloc(list_of_requed, ++num_of_requed * sizeof(thread));
            list_of_requed[num_of_requed - 1] = x->front->thr_ctr_blk_node->wait_process[j];
        }
        x->front = tempNode->next;
        if (munmap(tempNode->thr_ctr_blk_node->stkPtr, STK_SZ))
        {
            return errno;
        }
        if (x->front == NULL)
        {
            x->back = NULL;
        }
        free(tempNode->thr_ctr_blk_node->wait_process);
        free(tempNode->thr_ctr_blk_node);
        free(tempNode);
        x->length--;
        for (int i = 0; i < num_of_requed; i++)
        {
            thr_cntrl_blk *to_runnable = get_thread_from_queue(x, list_of_requed[i]);
            to_runnable->thr_state = RUNNABLE;
            re_queue(x, to_runnable);
        }
        free(list_of_requed);
        return 0;
    }
    else
    {
        if (tempNode->next == NULL)
        {
            return 0;
        }
        while (tempNode->next)
        {
            if (tempNode->next->thr_ctr_blk_node->tid == thr_id)
            {
                for (int j = 0; j < tempNode->next->thr_ctr_blk_node->numOfwait; j++)
                {
                    list_of_requed = (thread *)realloc(list_of_requed, ++num_of_requed * sizeof(thread));
                    list_of_requed[num_of_requed - 1] = tempNode->next->thr_ctr_blk_node->wait_process[j];
                }
                if (munmap(tempNode->next->thr_ctr_blk_node->stkPtr, STK_SZ ))
                {
                    return errno;
                }
                node_queue *deletedNode_ = tempNode->next;
                if (deletedNode_ == x->back)
                {
                    x->back = tempNode;
                }
                tempNode->next = deletedNode_->next;
                free(deletedNode_->thr_ctr_blk_node->wait_process);
                free(deletedNode_->thr_ctr_blk_node);
                free(deletedNode_);
                x->length--;
                for (int i = 0; i < num_of_requed; i++)
                {
                    thr_cntrl_blk *to_runnable = get_thread_from_queue(x, list_of_requed[i]);
                    to_runnable->thr_state = RUNNABLE;
                    re_queue(x, to_runnable);
                }
                free(list_of_requed);
                return 0;
            }
            tempNode = tempNode->next;
        }
    }
    return 0;
}
void swtch_cntxt(sigjmp_buf *old_cntxt, sigjmp_buf *new_cntxt)
{
    int retValue = sigsetjmp(*old_cntxt, 1);
    if (retValue == 0)
    {
        siglongjmp(*new_cntxt, 1);
    }
    return;
}
void pending_sig()
{
    if (!running_proc)
    {
        return;
    }
    int count = running_proc->num_unaddressed_sig;
    for (int s = 0; s < count; s++)
    {
        running_proc->num_unaddressed_sig -= 1;
        kill(getpid(), running_proc->remaining_signal[s]);
    }
}
void timer_on()
{
    if (signal(SIGVTALRM, swtchTosched) == SIG_ERR)
    {
        perror("Start timer");
        exit(EXIT_FAILURE);
    };
    return;
}
void swtchTosched()
{
    pending_sig();
    swtch_cntxt(running_proc->contxt, sched->contxt);
    timer_on();
}
void timer_off()
{
    if (signal(SIGVTALRM, SIG_IGN) == SIG_ERR)
    {
        perror("Stop timer");
        exit(EXIT_FAILURE);
    };
    return;
}
