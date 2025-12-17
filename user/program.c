#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define TOTAL_MEM (128ULL * 1024 * 1024) // Total memory in bytes
#define MAX_PROCS 64
#define ESC "\x1b"

// Generic function to draw any usage bar at a specific row
void draw_bar(int row, uint64 val, uint64 max) {
  int i;
  int len = 20;
  int filled;

  if (val > max)
    val = max;

  filled = (val * len) / max;

  // Move cursor to start of bar
  printf(ESC "[%d;1H[", row);

  for(i = 0; i < len; i++){
    if(i < filled)
      printf("█");
    else
      printf("░");
  }
  
  printf("] %d%%", (int)((val * 100) / max));
}

int main(void) {
  struct kstats stat;
  int pid;
  char c;

  // Fork a watcher process to handle input
  pid = fork();

  if(pid == 0) {
    // Child: Input watcher
    while(read(0, &c, 1) > 0) {
      if(c == '\n') { // \n is newline = Enter key
        close(open("quit", O_CREATE)); // Signal parent to quit
        exit(0);
      }
    }
    exit(0);
  }

  // Dashboard
  printf(ESC "[2J"); // Clear screen
  printf(ESC "[?25l"); // Hide cursor
  printf(ESC "[1;1H         Kernel Data Dashboard        ");
  printf(ESC "[2;1H======================================");

  // Main Loop: Runs until "quit" file exists
  while(open("quit", 0) < 0) {
    ugetstats(&stat);

    // Uptime
    printf(ESC "[3;1HSystem Uptime: %d s (%d ticks)", stat.uptime_ticks/10, stat.uptime_ticks);

    // Memory Usage Bar
    uint64 usedMem = TOTAL_MEM - stat.freemem;

    printf(ESC "[5;1HMemory Used:   %d / %d MB", (int)(usedMem / (1024 * 1024)), (int)(TOTAL_MEM / (1024 * 1024)));
    draw_bar(6, usedMem, TOTAL_MEM);

    // Process Used Bar 
    printf(ESC "[8;1HProcess Load:  %d / %d Slots", stat.total_procs, MAX_PROCS);
    draw_bar(9, stat.total_procs, MAX_PROCS);

    // Processes Details
    printf(ESC "[11;1H Running:  %d", stat.n_running);
    printf(ESC "[12;1H Runnable: %d", stat.n_runnable);
    printf(ESC "[13;1H Sleeping: %d", stat.n_sleeping);
    printf(ESC "[14;1H Zombie:   %d", stat.n_zombie);

    // Disk reads/writes Counts
    printf(ESC "[16;1HDisk I/O:    Reads: %ld | Writes: %ld ", stat.disk_reads, stat.disk_writes);
    //To test reads and writes count: Run ls or grep
    printf(ESC "[17;1H======================================");
    printf(ESC "[18;1H         Press Enter to exit          ");

    pause(5); //Update every 5 tick ~ 0.5 seconds
  }

  // Cleanup
  if(pid > 0)
    kill(pid); // Kill watcher
  
  unlink("quit");

  printf(ESC "[?25h"); // Reveal cursor back
  
  printf("\n\nDashboard closed.\n");
  
  exit(0);
}