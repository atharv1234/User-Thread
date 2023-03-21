
#include "lock.h"

int initialize_splk(spnlk *pass_lock)
{
    volatile int o_val;
    volatile int *locking_vari = &(pass_lock->lk);
    asm(
        "movl $0x0,(%1);"
        : "=r"(o_val)
        : "r"(locking_vari));
    pass_lock->lker = 0;
    return 0;
}

int acq_spnlk(spnlk *pass_lock)
{
    //busy wait until lock ready
    int o_val;
    volatile int *locking_vari = &(pass_lock->lk);
    asm(
        "whloop:"
        "xchg   %%al, (%1);"
        "test   %%al,%%al;"
        "jne whloop;"
        : "=r"(o_val)
        : "r"(locking_vari));
    return 0;
}


int relse_spnlk(spnlk *pass_lock)
{
    int o_val;
    volatile int *locking_vari = &(pass_lock->lk);
    asm(
        "movl $0x0,(%1);"
        : "=r"(o_val)
        : "r"(locking_vari));
    pass_lock->lker = 0;
    return 0;
}


int spnlk_try(spnlk *pass_lock)
{
    return pass_lock->lk == 0 ? 0 : EBUSY;
}



