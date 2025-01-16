#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");
    p->current_thread = 0;
    struct thread *t;
    for (t = p->threads; t < &p->threads[MAX_THREAD]; t++)
    {
      t->state = THREAD_FREE;
    }

    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
               (uint64)trampoline, PTE_R | PTE_X) < 0)
  {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
               (uint64)(p->trapframe), PTE_R | PTE_W) < 0)
  {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// Set up first user process.
void userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;     // user program counter
  p->trapframe->sp = PGSIZE; // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0)
  {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++)
  {
    if (pp->parent == p)
    {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);
  struct thread *t;
  for (t = myproc()->threads; t < &myproc()->threads[MAX_THREAD]; t++)
  {
    if (t->state != THREAD_FREE)
    {
      kfree(t->trapframe);
      t->state = THREAD_FREE;
    }
  }

  myproc()->current_thread = 0;
  freeproc(p);
  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++)
    {
      if (pp->parent == p)
      {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE)
        {
          // Found one.
          pid = pp->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                   sizeof(pp->xstate)) < 0)
          {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p))
    {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); // DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  struct proc *low_priority[NPROC];

  c->proc = 0;
  for (;;)
  {
    int low_priority_n = 0;
    struct proc *shortest_job = 0;
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting.
    intr_on();
    int found = 0;
    // start
    int min = 0;
    for (p = proc; p < &proc[NPROC]; p++)
    {
      if (p->state == RUNNABLE)
      {
        if (p->usage.sum_of_ticks >= p->usage.quota)
        {
          low_priority[low_priority_n++] = p;
          continue;
        }

        if (shortest_job == 0 || p->usage.sum_of_ticks < min)
        {
          shortest_job = p;
          min = p->usage.sum_of_ticks;
        }
        else if (p->usage.sum_of_ticks == min)
        { // handle tie with closer deadline
          if (p->usage.has_deadline)
          {
            if (shortest_job->usage.has_deadline)
            {
              if (p->usage.deadline < shortest_job->usage.deadline)
              {
                shortest_job = p;
                min = p->usage.sum_of_ticks;
              }
            }
            else
            {
              shortest_job = p;
              min = p->usage.sum_of_ticks;
            }
          }
        }
      }
    }

    // pick from low priority array, if no normal priority is runnable
    if (shortest_job == 0 && low_priority_n > 0)
    {
      for (int i = 0; i < low_priority_n; i++)
      {
        p = low_priority[i];
        if (shortest_job == 0 || p->usage.sum_of_ticks < min)
        {
          shortest_job = p;
          min = p->usage.sum_of_ticks;
        }
        else if (p->usage.sum_of_ticks == min)
        { // handle tie with closer deadline
          if (p->usage.has_deadline)
          {
            if (shortest_job->usage.has_deadline)
            {
              if (p->usage.deadline < shortest_job->usage.deadline)
              {
                shortest_job = p;
              }
            }
            else
            {
              shortest_job = p;
            }
          }
        }
      }
    }
    p = shortest_job;
    // end
    if (p != 0)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        // printf("\nvaredshod111  : %d\n", p->pid);
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        p->state = RUNNING;
        struct thread *t;
        if (p->current_thread != 0)
        {
          if (p->current_thread->state != THREAD_FREE)
          {
            *(p->current_thread->trapframe) = *(p->trapframe);
          }
          for (t = p->threads; t < &p->threads[4]; t++)
          {
            c->proc = p;
            p->state = RUNNING;
            if (t->state == THREAD_RUNNABLE)
            {
              t->state = THREAD_RUNNING;
              *(p->trapframe) = *(t->trapframe);
              p->current_thread = t;
              swtch(&c->context, &p->context);
              if (t->state != THREAD_FREE &&
                  t->state != THREAD_JOINED)
              {
                t->state = THREAD_RUNNABLE;
                *(t->trapframe) = *(p->trapframe);
              }
            }
          }
        }
        else
        {
          if (myproc()->usage.start_tick == -1)
          {
            myproc()->usage.start_tick = ticks;
          }
          uint temp = ticks;
          swtch(&c->context, &p->context);
          myproc()->usage.sum_of_ticks += ticks - temp;
          for (struct proc *p = proc; p < &proc[NPROC]; p++)
          {
            if (p->killed != 1 && p->usage.has_deadline)
            {
              if (p->usage.deadline <= ticks)
              {
                if (p->state == SLEEPING)
                  p->state = RUNNABLE;
                p->killed = 1;
                printf("id: %d\n", p->pid);
              }
            }
          }
          // printf("sch id: %d %d  %d %d\n", myproc()->pid, ticks, myproc()->usage.start_tick,myproc()->usage.sum_of_ticks);
        }

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if (found == 0)
    {
      // nothing to run; stop running on this core until an interrupt.
      intr_on();
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first)
  {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock); // DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p != myproc())
    {
      acquire(&p->lock);
      if (p->state == SLEEPING && p->chan == chan)
      {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst)
  {
    return copyout(p->pagetable, dst, src, len);
  }
  else
  {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src)
  {
    return copyin(p->pagetable, dst, src, len);
  }
  else
  {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [USED] "used",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

void find_decessors(struct child_processes *cp, int pid)
{
  struct proc *thisProc;

  for (thisProc = &proc[0]; thisProc < &proc[NPROC]; thisProc++)
  {
    acquire(&thisProc->lock);
    release(&thisProc->lock);
    struct proc *thisPP;
    int thisPPPid = -1;
    if (thisProc->parent)
    {
      acquire(&wait_lock);
      thisPP = thisProc->parent;
      release(&wait_lock);
      acquire(&thisPP->lock);
      thisPPPid = thisPP->pid;
      release(&thisPP->lock);
    }

    if (thisPPPid == pid)
    {
      struct proc_info child;
      acquire(&thisProc->lock);
      strncpy(child.name, thisProc->name, 16);
      child.pid = thisProc->pid;
      child.ppid = pid;
      child.state = thisProc->state;
      release(&thisProc->lock);
      cp->processes[cp->count] = child;
      cp->count++;
      find_decessors(cp, child.pid);
    }
  }
}

void find_children(struct child_processes *cp)
{
  cp->count = 0;
  struct proc *p = myproc();
  acquire(&p->lock);
  int pid = p->pid;
  release(&p->lock);
  find_decessors(cp, pid);
}

struct
{
  struct report reports[MAX_REPORT_BUFFER_SIZE];
  int numberOfReports;
  int writeIndex;
} _internal_report_list;

void add_report_traps(char *pname, int pid, uint scause, uint sepc, uint stval)
{
  strncpy(_internal_report_list.reports[_internal_report_list.writeIndex].pname, pname, sizeof(pname));
  _internal_report_list.reports[_internal_report_list.writeIndex].pid = pid;
  _internal_report_list.reports[_internal_report_list.writeIndex].scause = scause;
  _internal_report_list.reports[_internal_report_list.writeIndex].stval = stval;
  _internal_report_list.reports[_internal_report_list.writeIndex].sepc = sepc;
  _internal_report_list.writeIndex++;
  _internal_report_list.numberOfReports++;

  if (_internal_report_list.writeIndex >= MAX_REPORT_BUFFER_SIZE)
  {
    _internal_report_list.writeIndex = 0;
  }

  if (_internal_report_list.numberOfReports >= MAX_REPORT_BUFFER_SIZE)
  {
    _internal_report_list.numberOfReports = MAX_REPORT_BUFFER_SIZE;
  }
}
void report_traps(struct report_traps *traps)
{
  struct child_processes children_procs;
  find_children(&children_procs);
  printf("test\n");
  traps->count = 0;

  for (int i = 0; i < _internal_report_list.numberOfReports; i++)
  {
    for (int j = 0; j < children_procs.count; j++)
    {
      if (_internal_report_list.reports[i].pid == children_procs.processes[j].pid ||
          _internal_report_list.reports[i].pid == children_procs.processes[j].ppid)
      {
        strncpy(traps->reports[traps->count].pname, _internal_report_list.reports[i].pname, sizeof(_internal_report_list.reports[i].pname));
        traps->reports[traps->count].pid = _internal_report_list.reports[i].pid;
        traps->reports[traps->count].scause = _internal_report_list.reports[i].scause;
        traps->reports[traps->count].sepc = _internal_report_list.reports[i].sepc;
        traps->reports[traps->count].stval = _internal_report_list.reports[i].stval;
        traps->count++;
      }
    }
  }
}

int thread_num = 1;
struct spinlock thread_id_lock;

int create_thread(void *(*function)(void *), void *arg, void *stack)
{
  struct proc *thisProcess = myproc();
  struct thread *t;
  struct thread *mainThread;
  if (thisProcess->current_thread == 0)
  {
    mainThread = &thisProcess->threads[0];
    mainThread->trapframe = (struct trapframe *)kalloc();
    if (mainThread->trapframe == 0)
    {
      return -1;
    }
    *(mainThread->trapframe) = *(thisProcess->trapframe);
    mainThread->id = thread_num;
    thread_num++;
    mainThread->state = THREAD_RUNNABLE;
    mainThread->join = 0;
    thisProcess->current_thread = mainThread;
  }
  for (t = thisProcess->threads; t < &thisProcess->threads[4]; t++)
  {
    if (t->state == THREAD_FREE)
    {
      t->trapframe = (struct trapframe *)kalloc();
      if (t->trapframe == 0)
      {
        return -1;
      }
      memset(t->trapframe, 0, sizeof(*t->trapframe));
      t->trapframe->a0 = (uint64)arg;
      t->trapframe->epc = (uint64)function; // entry program counter >:)
      t->trapframe->sp = (uint64)(stack) + 1024;
      t->trapframe->ra = (uint64)-1;
      t->state = THREAD_RUNNABLE;
      t->join = 0;
      t->id = thread_num;
      thread_num++;

      return t->id;
    }
  }
  return -1;
}

int join_thread(struct thread *thisThread, int id)
{
  struct thread *t;
  for (t = myproc()->threads; t < &myproc()->threads[4]; t++)
  {
    if (t->id == id && t->state != THREAD_FREE)
    {
      thisThread->state = THREAD_JOINED;
      thisThread->join = id;
      yield(); // back to scheduler
      return 0;
    }
  }
  return -1;
}

int stop_thread(int id)
{
  struct proc *p = myproc();
  struct thread *t;
  printf("Thread number %d is stopping id: %d\n", p->current_thread->id, id);

  for (t = p->threads; t <= &p->threads[3]; t++)
  {
    if (t->id == id && t->state != THREAD_FREE)
    {
      if (t->state == THREAD_RUNNING || t->state == THREAD_RUNNABLE)
      {
        t->state = THREAD_FREE;
        kfree(t->trapframe);
        t->trapframe = 0;
        yield();
        return 0;
      }
    }
  }
  return -1;
}

void calcu_usage(struct proc_info *po)
{
  po->pid = myproc()->pid;
  strncpy(po->name, myproc()->name, sizeof(myproc()->name));
  po->ppid = myproc()->parent->pid;
  po->state = myproc()->state;
  po->usage = myproc()->usage;
  return;
}

int total_usages(struct top *t)
{
  struct proc_info pi;
  struct proc *p;
  t->count = 0;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state != UNUSED)
    {
      strncpy(pi.name, p->name, sizeof(p->name));
      pi.pid = p->pid;
      if (p->parent)
        pi.ppid = p->parent->pid;
      else
        pi.ppid = -1;
      pi.state = p->state;
      pi.usage = p->usage;
      t->processes[t->count++] = pi;
    }
    release(&p->lock);
  }
  return 0;
}

int cpu_set_quota(int pid, int quota)
{
  struct child_processes child_proc;
  find_children(&child_proc);
  struct proc *p;
  int is_found = 0;
  if (myproc()->pid == pid)
  {
    myproc()->usage.quota = quota;
    is_found = 1;
  }

  for (int i = 0; i < child_proc.count; i++)
  {
    if (child_proc.processes[i].pid == pid)
    {
      for (p = proc; p < &proc[NPROC]; p++)
      {
        if (p->pid == pid)
        {
          p->usage.quota = quota;
          is_found = 1;
        }
      }
    }
  }
  if (is_found)
    return 0;
  else
    return -1;
}

int fork2(int deadline)
{
  int pid = fork();
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (pid == p->pid)
    {
      p->usage.deadline = deadline + ticks;
      p->usage.has_deadline = 1;
      release(&p->lock);
      return pid;
    }
    release(&p->lock);
  }
  return -1;
}