#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[]; // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if (*pde & PTE_P)
  {
    pgtab = (pte_t *)P2V(PTE_ADDR(*pde));
  }
  else
  {
    if (!alloc || (pgtab = (pte_t *)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char *)PGROUNDDOWN((uint)va);
  last = (char *)PGROUNDDOWN(((uint)va) + size - 1);
  for (;;)
  {
    if ((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if (*pte & PTE_P && (*pte & PTE_W))
      panic("remap");
    *pte = pa | perm | PTE_P;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

void tvinit(void)
{
  int i;

  for (i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void idtinit(void)
{
  lidt(idt, sizeof(idt));
}

// PAGEBREAK: 41
void trap(struct trapframe *tf)
{
  if (tf->trapno == T_SYSCALL)
  {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno)
  {
  case T_IRQ0 + IRQ_TIMER:
    if (cpuid() == 0)
    {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE + 1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:
    // cprintf("pid: %d name: %s cr2: %x size: %x\n", myproc()->pid, myproc()->name, rcr2(), myproc()->sz);
    if (rcr2() <= KERNBASE)
    {
      // bring the page to the page table
      uint va = rcr2();
      struct proc *curproc = myproc();
      pte_t *pte = walkpgdir(curproc->pgdir, (void *)va, 0);
      // handler for demand paging
      if (!(PTE_FLAGS(*pte) & PTE_P))
      {
        cprintf("page fault occurred, doing demand paging for address: 0x%x\n", rcr2());
        char *mem = kalloc();
        memset(mem, 0, PGSIZE);
        if (PTE_FLAGS(*pte) & PTE_W)
          mappages(myproc()->pgdir, (char *)PTE_ADDR(rcr2()), PGSIZE, V2P(mem), PTE_W | PTE_U);
        else
          mappages(myproc()->pgdir, (char *)PTE_ADDR(rcr2()), PGSIZE, V2P(mem), PTE_U);
      }
      // handler for copy on write fault
      else if (!(PTE_FLAGS(*pte) & PTE_W))
      {
        cprintf("%s: page fault occurred due to copy on write while accessing address: 0x%x\n", curproc->name, va);
        uint pa = PTE_ADDR(*pte);
        uint refCount = getreferences(pa);
        if (refCount > 1)
        {
          char *mem = kalloc();
          memmove(mem, (char *)P2V(pa), PGSIZE);
          *pte = (V2P(mem)) | PTE_W | PTE_P | PTE_U;
          lcr3(V2P(curproc->pgdir));
          decrementreferences(pa);
        }
        else if (refCount == 1)
        {
          *pte = *pte | PTE_W;
        }
        else
        {
          cprintf("Illegal\n");
          break;
        }
      }
    }
    else
    {

      pte_t *pte;
      uint va = rcr2();
      struct proc *curproc = myproc();

      if ((pte = walkpgdir(curproc->pgdir, (void *)va, 0)) == 0)
        cprintf("No entry\n");
      cprintf("Here\n");
      cprintf("%s: pid: %d Error va: %x flags: %x\n", curproc->name, curproc->pid, *pte, PTE_FLAGS(*pte));
      cprintf("pid %d %s: trap %d err %d on cpu %d "
              "eip 0x%x addr 0x%x--kill proc\n",
              myproc()->pid, myproc()->name, tf->trapno,
              tf->err, cpuid(), tf->eip, rcr2());
      myproc()->killed = 1;
    }
    break;

  // PAGEBREAK: 13
  default:
    if (myproc() == 0 || (tf->cs & 3) == 0)
    {
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if (myproc() && myproc()->state == RUNNING &&
      tf->trapno == T_IRQ0 + IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}
