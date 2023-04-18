#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

void sys_pgtPrint2(pde_t *pde)
{
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
}

void find_readonly_segments(char *elf_file)
{
  struct proc *currproc = myproc();
  cprintf("%s\n", currproc->name);
  struct proghdr *ph, *eph;
  struct elfhdr *elf;
  elf = (struct elfhdr *)elf_file;
  ph = (struct proghdr *)(elf_file + elf->phoff);
  eph = ph + elf->phnum;

  for (; ph < eph; ph++)
  {
    if (ph->flags & ELF_PROG_FLAG_EXEC)
      continue;
    if (ph->flags & ELF_PROG_FLAG_WRITE)
      continue;
    cprintf("Read-only segment found:\n");
    cprintf("Virtual address: 0x%lx\n", ph->vaddr);
    cprintf("Size in memory: %d bytes\n", ph->memsz);
  }
}

int exec(char *path, char **argv)
{
  // cprintf("Starting exec of %s %s\n", path, *(argv));
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3 + MAXARG + 1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  begin_op();

  if ((ip = namei(path)) == 0)
  {
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;
  // Check ELF header
  if (readi(ip, (char *)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if (elf.magic != ELF_MAGIC)
    goto bad;

  if ((pgdir = setupkvm()) == 0)
    goto bad;

  // find_readonly_segments((char *)&elf);
  // Load program into memory.
  sz = 0;
  for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph))
  {
    if (readi(ip, (char *)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.memsz < ph.filesz)
      goto bad;
    if (ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if ((sz = allocuvm(pgdir, sz, ph.vaddr + ph.filesz)) == 0)
      goto bad;
    if (ph.vaddr % PGSIZE != 0)
      goto bad;
    if (loaduvm(pgdir, (char *)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  // cprintf("First\n");
  // sys_pgtPrint2(pgdir);
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if ((sz = allocuvm(pgdir, sz, sz + 2 * PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char *)(sz - 2 * PGSIZE));
  sp = sz;

  // cprintf("Second\n");
  // sys_pgtPrint2(pgdir);

  // Push argument strings, prepare rest of stack in ustack.
  for (argc = 0; argv[argc]; argc++)
  {
    if (argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if (copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3 + argc] = sp;
  }
  ustack[3 + argc] = 0;

  ustack[0] = 0xffffffff; // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc + 1) * 4; // argv pointer

  sp -= (3 + argc + 1) * 4;
  if (copyout(pgdir, sp, ustack, (3 + argc + 1) * 4) < 0)
    goto bad;

  // Save program name for debugging.
  for (last = s = path; *s; s++)
    if (*s == '/')
      last = s + 1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry; // main
  curproc->tf->esp = sp;
  switchuvm(curproc);
  freevm(oldpgdir);
  return 0;

bad:
  if (pgdir)
    freevm(pgdir);
  if (ip)
  {
    iunlockput(ip);
    end_op();
  }
  return -1;
}
