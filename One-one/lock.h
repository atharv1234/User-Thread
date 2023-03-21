#include <sys/syscall.h>    /* Definition of SYS_* constants */
#include <linux/futex.h>      /* Definition of FUTEX_* constants */
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <errno.h>
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
typedef struct 
{
    volatile int lk;
    unsigned int lker;
} spnlk;

int initialize_splk(spnlk *);

int acq_spnlk(spnlk *);

int relse_spnlk(spnlk *);

int spnlk_try(spnlk *);
