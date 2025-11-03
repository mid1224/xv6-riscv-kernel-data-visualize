// this file is included by both kernel code (proc.c and sysproc.c)
// and user-space code (program.c)
// this is to define where we put the data

#ifndef _KSTATS_H_ // ifnotdefined, prevent the header's contents 
                   // from being processed multiple times
#define _KSTATS_H_

#include "types.h" //For uint64

// This struct defines the data that will be passed
// from the kernel to the user-space dashboard.
struct kstats {
  uint64 freemem;      // Amount of free memory in bytes

  // Process counts by state
  int n_runnable;      // Number of processes in RUNNABLE state
  int n_sleeping;      // Number of processes in SLEEPING state
  int n_zombie;        // Number of processes in ZOMBIE state
  int n_running;       // Number of processes in RUNNING state
  int total_procs;     // Total processes in the table (not UNUSED)

  // TO BE ADDED (?)
  // uint64 uptime_ticks; // Kernel ticks since boot
};

#endif // _KSTATS_H_