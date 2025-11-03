#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "kstats.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
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
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
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

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
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

// !!! custom program here
uint64
sys_program(void)
{
  // the kernel side program lives here

  struct proc *p = myproc();

  printf("kernel: program() called by pid %d (running in kernel)\n", p->pid);

  
  struct kstats ks; // Declare a struct to store the data
  
  // Call our data-gathering functions
  ks.freemem = kfreemem();
  countprocs(&ks);
  
  // Print the results to the kernel console
  printf("kernel: Free Memory (in bytes): %ld\n", ks.freemem);
  printf("kernel: Free Memory (in megabytes): %ld\n", ks.freemem/(1024 * 1024)); //Converted from byte to megabyte for easier reading
  printf("kernel: Total Procs: %d\n", ks.total_procs);
  printf("kernel: Runnable: %d\n", ks.n_runnable);
  printf("kernel: Sleeping: %d\n", ks.n_sleeping);
  printf("kernel: Zombie: %d\n", ks.n_zombie);
  printf("kernel: Running: %d\n", ks.n_running);

  return 0;
}
