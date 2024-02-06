#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
    uint8 buf[1024] = { 0 };
    struct proc* p = myproc();

    uint64 u_base;
    int len;
    uint64 u_mask;

    argaddr(0, &u_base);
    argint(1, &len);
    argaddr(2, &u_mask);

    if (len > sizeof(buf) * 8) {
        return -1;
    }

    for (uint i = 0; i < (len < PGSIZE ? len : PGSIZE); ++i) {
        if (accessed(p->pagetable, u_base + PGSIZE * i)) {
            buf[i / 8] |= (1 << (i % 8)); // set i's bit in data
        }
    }

    if (copyout(p->pagetable, u_mask, (char*)buf, (len-1) / 8 + 1) < 0) {
        return -1;
    }

    return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
