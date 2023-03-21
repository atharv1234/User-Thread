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
#include "lock.h"
#define STK_SZ 51200
#define CFLAGS CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD |CLONE_SYSVSEM|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID
spnlk splock;


typedef struct fun_arg
{
    void (*f)(void *); //function pointer
    void *argmnt; // arguements passed to thread
    void *stack; // stack for each thread
} fun_arg;

//Applications should call either sigemptyset() or sigfillset() at least once for each 
//object of type sigset_t prior to any other use of that object. If such an object is not 
//initialized in this way, but is nonetheless supplied as an argument to any of pthread_sigmask(), 
//sigaction(), sigaddset(), sigdelset(), sigismember(), sigpending(), sigprocmask(), 
//sigsuspend(), sigtimedwait(), sigwait(), or sigwaitinfo(), the results are undefined.
void at_sig_handler(int sig_number)
{
    printf("Sent signal");
}
typedef unsigned long thread; //thread object
//thread node
typedef struct thread_node
{
    unsigned long int tid;
    struct thread_node *next;
    void *return_val;
    fun_arg *fArg;
} thread_node;

typedef struct thread_attributes
{
    void *stack;
    size_t stack_size;
    size_t protect_size;
} thread_attributes;



//single linked list for thread controlling
typedef struct single_LL
{
    thread_node *head;
    thread_node *tail;
} single_LL;

spnlk glblck;
single_LL tid_list;

int initialize_sll(single_LL *t)
{
    if (!t)
        return -1;
    t->head = t->tail = NULL;
    return 0;
}

unsigned long int *get_tid_address(single_LL *x, unsigned long int tid)
{
    thread_node *temp_node = x->head;
    while (temp_node != NULL)
    {
        if (temp_node->tid == tid)
        {
            return &(temp_node->tid);
        }
        temp_node = temp_node->next;
    }
    return 0;
}
//delete all threads after exit

thread_node *get_particular_tid_node(single_LL *x, unsigned long int thr_id)
{
    thread_node *t = x->head;
    while (t != NULL)
    {
        if (t->tid == thr_id)
        {
            return t;
        }
        t = t->next;
    }
    return NULL;
}



// PROT_READ
//         Pages may be read.

// PROT_WRITE
//         Pages may be written.

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
}


thread_node* insert_sll(single_LL *x,unsigned long int tid)
{
    //insert at the tail
    thread_node *temp;
    if (posix_memalign((void **)&temp, 8, sizeof(thread_node)))
    {
        perror("linked list allocation");
        return NULL;
    }
    temp->tid = tid;
    temp->next = NULL;
    temp->return_val = NULL;
    if (x->head == NULL)
    {
        x->head = x->tail = temp;
    }
    else
    {
        x->tail->next = temp;
        x->tail = temp;
    }
    return temp;
}

int delete_sll(single_LL *x,unsigned long int tid)
{
    thread_node *temp_node = x->head;
    if (temp_node == NULL)
    {
        return -1;
    }
    //locate the node with passed tid and free the memory
    if (temp_node->tid == tid)
    {
        x->head = x->head->next;
        if (temp_node->fArg)
        {
            //unmap the function args for thread with tid
            //getpagesize Return the number of bytes in a page. This is the system's page size, which is not necessarily the same as the hardware page size.
            if (munmap(temp_node->fArg->stack, STK_SZ + getpagesize()))
            {
                return errno;
            }
        }
        free(temp_node->fArg);
        free(temp_node);
        if (x->head == NULL)
        {
            x->tail = NULL;
        }
        return 0;
    }
    while (temp_node->next)
    {
        if (temp_node->next->tid == tid)
        {
            thread_node *next_temp = temp_node->next->next;
            if (temp_node->next == x->tail)
            {
                x->tail = temp_node;
            }
            if (temp_node->next->fArg)
            {
                if (munmap(temp_node->next->fArg->stack, STK_SZ + getpagesize()))
                {
                    return errno;
                }
            }
            free(temp_node->next->fArg);
            free(temp_node->next);
            temp_node->next = next_temp;
            break;
        }
        temp_node = temp_node->next;
    }
    return 0;
}

void exit_thread(void *retval)
{
    acq_spnlk(&glblck);
    void *tid_addr = get_tid_address(&tid_list, gettid());
    if (tid_addr == NULL)
    {
        relse_spnlk(&glblck);
        return;
    }
    if (retval)
    {
        thread_node *added_node = get_particular_tid_node(&tid_list, gettid());
        added_node->return_val = retval;
    }
    syscall(SYS_futex, tid_addr, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
    relse_spnlk(&glblck);
    kill(SIGINT,gettid());
    return;
}

static int wrapper(void *fun_ar)
{
    fun_arg *tempArg;
    tempArg = (fun_arg *)fun_ar;
    tempArg->f(tempArg->argmnt);
    exit_thread(NULL);
}
static void initialize_oneone()
{
    initialize_splk(&glblck);
    initialize_sll(&tid_list);
    thread_node *added_node = insert_sll(&tid_list, gettid());
    added_node->fArg = NULL;
//     del_threads();
    return;
}

int create_thread(thread *thr, void *attribute, void *functions, void *argmnt)
{
    //if first then initialize singly ll
    //insert the node and pass function arguements
    //check the attributes and and clone a thread
    acq_spnlk(&glblck);
    static int initial_state = 0;
    if (thr == NULL || functions == NULL)
    {
        relse_spnlk(&glblck);
        return EINVAL;
    }
    thread tid;
    void *thread_stack;
    if (initial_state == 0)
    {
        initial_state = 1;
        initialize_oneone();
        
    }
    thread_node *added_node = insert_sll(&tid_list, 0);
    if (added_node == NULL)
    {
        printf("No address found.\n");
        relse_spnlk(&glblck);
        return -1;
    }
    fun_arg *fArg;
    fArg = (fun_arg *)malloc(sizeof(fun_arg));
    if (!fArg)
    {
        printf("Failed allocation memory\n");
        relse_spnlk(&glblck);
        return -1;
    }
    fArg->f = functions;
    fArg->argmnt = argmnt;
    if (attribute)
    {
        thread_attributes *atr = (thread_attributes *)attribute;
        thread_stack = atr->stack == NULL ? stack_allocation(atr->stack_size, atr->protect_size) : atr->stack;
        if (!thread_stack)
        {
            perror("stack allocation");
            relse_spnlk(&glblck);
            return errno;
        }
        fArg->stack = atr->stack == NULL ? thread_stack : atr->stack;
        tid = clone(wrapper,
                    thread_stack + ((thread_attributes *)attribute)->stack_size + ((thread_attributes *)attribute)->protect_size,
                    CFLAGS,
                    (void *)fArg,
                    &(added_node->tid),
                    NULL,
                    &(added_node->tid));
        
        added_node->tid = tid;
        added_node->fArg = fArg;
    }
    else
    {
        thread_stack = stack_allocation(STK_SZ, 0);
        if (!thread_stack)
        {
            perror("stack allocation");
            relse_spnlk(&glblck);
            return errno;
        }
        fArg->stack = thread_stack;
      
        tid = clone(wrapper,
                    thread_stack + STK_SZ,
                    CFLAGS,
                    (void *)fArg,
                    &(added_node->tid),
                    NULL,
                    &(added_node->tid));
        added_node->tid = tid;
        added_node->fArg = fArg;
    }
    if (tid == -1)
    {
        perror("thread creation");
        free(thread_stack);
        relse_spnlk(&glblck);
        return errno;
    }
    *thr = tid;
    relse_spnlk(&glblck);
    return 0;
}

//function sends the signal sig to thread
int kill_thread(pid_t tid, int sig_number)
{
    if (sig_number == 0)
    {
        return -1;
    }
    int ret_val;
    thread_node *in_node = get_particular_tid_node(&tid_list, tid);
    if (sig_number == SIGINT || sig_number == SIGCONT || sig_number == SIGSTOP)
    {
        //kill all thread
        single_LL * l = &tid_list;
        thread_node *temp_pointer = l->head;
        pid_t par_id = getpid();
        int retn_val;
        pid_t deleted_pid[100];
        int c = 0;
        while (temp_pointer)
        {
            if (temp_pointer->tid == gettid())
            {
                temp_pointer = temp_pointer->next;
                continue;
            }
            printf("Thread kill: %ld\n", temp_pointer->tid);
            retn_val = syscall(234, par_id, temp_pointer->tid, sig_number);
            if (retn_val == -1)
            {
                perror("tkill");
                return errno;
            }
            else
            {
                if (sig_number == SIGINT || sig_number == SIGKILL)
                {
                    deleted_pid[c++] = temp_pointer->tid;
                }
            }
            temp_pointer = temp_pointer->next;
        }
        if (sig_number == SIGINT || sig_number == SIGKILL)
        {
            for (int i = 0; i < c; i++)
                delete_sll(&tid_list, deleted_pid[i]);
        }
        pid_t parid = getpid();
        ret_val = syscall(234, parid, gettid(), sig_number);
        if (ret_val == -1)
        {
            perror("tkill");
            return ret_val;
        }
        return ret_val;
    }
    if (in_node->tid == 0)
    {
        return -1;
    }
    pid_t pid = getpid();
    ret_val = syscall(234, pid, tid, sig_number);
    if (ret_val == -1)
    {
        perror("tkill");
        return ret_val;
    }
    return ret_val;
}

// The exit_thread() function terminates the calling thread and returns a
//        value via retval that (if the thread is joinable) is available  to  an‐
//        other thread in the same process that calls pthread_join


//wait for a specific thread to terminate
//t TID of the thread to wait for
// FUTEX_WAKE This operation wakes at most val of the waiters that are
//               waiting (e.g., inside FUTEX_WAIT) on the futex word at the
//               address uaddr.  Most commonly, val is specified as either
//               1 (wake up a single waiter) or INT_MAX (wake up all
//               waiters).
// FUTEX_WAIT (since Linux 2.6.0)
//               This operation tests that the value at the futex word
//               pointed to by the address uaddr still contains the
//               expected value val, and if so, then sleeps waiting for a
//               FUTEX_WAKE operation on the futex word.
int join_thread(thread thr, void **retval)
{
    acq_spnlk(&glblck);
    void *addr_val = get_tid_address(&tid_list, thr);
    if (addr_val == NULL)
    {
        relse_spnlk(&glblck);
        return ESRCH;
    }
    int retn_Val;
    while (*((pid_t *)addr_val) == thr)
    {
        relse_spnlk(&glblck);
        retn_Val = syscall(SYS_futex, addr_val, FUTEX_WAIT, thr, NULL, NULL, 0);
        acq_spnlk(&glblck);
    }
    syscall(SYS_futex, addr_val, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
    if (retval)
    {
        thread_node *added_node = get_particular_tid_node(&tid_list, thr);
        *retval = added_node->return_val;
    }
    relse_spnlk(&glblck);
  delete_sll(&tid_list,thr);
    return retn_Val;
}
void func()
{
    printf("Inside test function\n");
    exit_thread(NULL);
    return;
}
int i=0;
void rout1()
{
    acq_spnlk(&splock);
    usleep(500000);
    relse_spnlk(&splock);
  
}
void rout2()
{
    while (spnlk_try(&splock) == EBUSY)
    {
        i++;
    }
}

void rout3()
{
    while ((&splock)->lk != 0)
    {
        i++;
    };
    relse_spnlk(&splock);
}
void print_nodes(single_LL *l)
{
    thread_node *tmp = l->head;
    while (tmp)
    {
        if (tmp->fArg)
        {
            printf("tid%ld-->", tmp->tid);
        }
        tmp = tmp->next;
    }
    printf("\n");
    return;
}
int main()
{
    thread t;
    printf("Create with wrong arguements\n");
    if (create_thread(NULL, NULL, NULL, NULL) == EINVAL && create_thread(&t, NULL, NULL, NULL) == EINVAL && create_thread(NULL, NULL, func, NULL) == EINVAL)
    {
        printf("Test passed\n");
    }
    else
    {
        printf("Test failed\n");
    }
    printf("Join with wrong arguements\n");
    create_thread(&t, NULL, func, NULL);
    // Already joined thread
    join_thread(t, NULL);
    // Give any tid or rejoined threads
    if (join_thread(t, NULL) == ESRCH)
    {
        printf("Test passed\n");
    }
    else
    {
        printf("Test failed\n");
    }

    printf("Inappropriate signals\n");
    if (kill_thread(t, 0) == -1)
        printf("Test passed\n");
    else
        printf("Test failed\n");
  
    printf("Trylock test\n");
    initialize_splk(&splock);
    thread t3, t4, t5, t6;
    create_thread(&t3, NULL, rout1, NULL);
  create_thread(&t6, NULL, rout3, NULL);
    join_thread(t3, NULL);
  printf("Not using try: %d\n", i);
  i=0;
  create_thread(&t3, NULL, rout1, NULL);
    create_thread(&t4, NULL, rout2, NULL);
  join_thread(t3, NULL);
    printf("Using try: %d\n", i);
  
}
