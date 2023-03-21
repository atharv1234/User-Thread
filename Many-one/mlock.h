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
#include <linux/futex.h>
#include <linux/unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <limits.h>
#include <stdlib.h>
#include <setjmp.h>
#define STK_SZ  5120


#ifdef __x86_64__
#define JB_BP 5
#define JB_SP 6
#define JB_PC 7
#else
#define JB_BP 3
#define JB_SP 4
#define JB_PC 5
#endif
enum thr_state
{
    RUNNING,
    WAITING,
    RUNNABLE
};
typedef struct sched_parameters
{
    unsigned int st;
    unsigned int mx;
} sched_parameters;
typedef struct mutxlk
{
    volatile int lk;
    unsigned int lker;
} mutxlk;
typedef unsigned long int thread;
typedef unsigned long addr;
typedef struct thread_attributes
{
    void *stack;
    size_t stack_size;
    sched_parameters schd_duration;
    
} thread_attributes;
typedef struct fun_arg
{
    void (*f)(void *); //function pointer
    void *argmnt; // arguements passed to thread
} fun_arg;


typedef struct thr_cntrl_blk
{
    thread tid;
    void *stkPtr;
    size_t stkSz;
    int thr_state;
    sigjmp_buf *contxt;
    int ext_process;   //exited process
    int *wait_process; //waiters
    int numOfwait;
    mutxlk *wait_mutex; 
    int *remaining_signal;
    int num_unaddressed_sig;
    fun_arg *argmnt;
} thr_cntrl_blk;

typedef struct node_queue
{
    thr_cntrl_blk *thr_ctr_blk_node;
    struct node_queue *next;
} node_queue;


typedef struct q_tcb
{
    node_queue *front;
    node_queue *back;
    int length;
} q_tcb;
extern thr_cntrl_blk *running_proc;

extern thr_cntrl_blk *sched;

extern q_tcb thread_set;
int initialize_mutex(mutxlk *);

int acq_mtx(mutxlk *);

int relse_mtx(mutxlk *);

int try_mtx(mutxlk *);

int rem_thread(q_tcb *x, unsigned long int thr_id);
void re_queue(q_tcb *x, thr_cntrl_blk *thr_tcb);
int add_thread_to_queue(q_tcb *x, thr_cntrl_blk *thr_tcb);
thr_cntrl_blk *get_thread_from_queue(q_tcb *x, thread thr_id);
void swtch_cntxt(sigjmp_buf *old_cntxt, sigjmp_buf *new_cntxt);
