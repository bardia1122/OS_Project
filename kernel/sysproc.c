#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
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
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

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

uint64
sys_chp(void)
{
  struct child_processes *cp;
  struct child_processes kcp;
  argaddr(0, (uint64 *)&cp);
  find_children(&kcp);
  struct proc *p = myproc();
  if (p == 0)
  {
    return -1;
  };
  if (copyout(p->pagetable, (uint64)cp, (char *)&kcp, sizeof(kcp)) < 0)
    return -1;
  return 0;
}

uint64
sys_report(void)
{
  uint64 po;
  argaddr(0, &po);
  struct report_traps traps;
  report_traps(&traps);
  copyout(myproc()->pagetable, po, (char *)&traps, sizeof(traps));
  return 0;
}

uint64
sys_cthread(void)
{
  uint64 func_ptr, arg_ptr, stack;
  argaddr(0, (uint64 *)&func_ptr);
  argaddr(1, (uint64 *)&arg_ptr);
  argaddr(2, (uint64 *)&stack);
  return create_thread((void *)func_ptr, (void *)arg_ptr, (void *)stack);
}

uint64
sys_jthread(void)
{
  uint64 id;
  argaddr(0, &id);
  join_thread(myproc()->current_thread, id);
  return 0;
}

uint64
sys_sthread(void)
{
  uint64 id;
  argaddr(0, &id);
  stop_thread(id);
  printf("aa\n");
  return 0;
}

uint64
sys_cpu_usage(void)
{
  uint64 p;
  argaddr(0, &p);
  struct proc_info po;
  calcu_usage(&po);
  copyout(myproc()->pagetable, p, (char *)&po, sizeof(po));
  return 0;
}

uint64
sys_top(void)
{
  uint64 t;
  argaddr(0, &t);
  struct top to;
  total_usages(&to);
  copyout(myproc()->pagetable, t, (char *)&to, sizeof(to));
  return 0;
}

uint64
sys_set_quota(void)
{
  uint64 p;
  argaddr(0, &p);
  uint64 q;
  argaddr(1, &q);
  cpu_set_quota(p, q);
  return 0;
}
uint64
sys_fork2(void)
{
  uint64 l;
  argaddr(0, &l);
  int pid = fork2(l);
  return pid;
}