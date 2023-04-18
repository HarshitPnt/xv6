#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

int sys_date(void)
{
  struct rtcdate *dat;
  if (argptr(0, (void *)&dat, sizeof(*dat)) < 0)
    return -1;
  cmostime(dat);
  return 0;
}

int sys_pgtPrint(void)
{
  struct proc *currproc = myproc();
  pde_t *pde = currproc->pgdir;
  for (int i = 0; i < NPDENTRIES; ++i)
  {
    pte_t *inner_addr = (pte_t *)P2V(PTE_ADDR(*pde));
    if ((PTE_FLAGS(*pde) & PTE_P) && (PTE_FLAGS(*pde) & PTE_U))
    {
      pte_t *curr = inner_addr;
      for (int j = 0; j < NPTENTRIES; ++j)
      {
        if ((PTE_FLAGS(*curr) & PTE_P) && (PTE_FLAGS(*curr) & PTE_U))
          cprintf("Entry No: %x Virtual Address: 0x%x Physical Address: 0x%x\n", i * NPTENTRIES + j, PGADDR(i, j, 0), *curr);
        curr += 1;
      }
    }
    pde += 1;
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
