#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define TOTAL_MEM (128ULL * 1024 * 1024)
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
  
  printf("] %d%%          ", (int)((val * 100) / max));
}

int main(void) {
  struct kstats s;
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
  printf(ESC "[1;1H        XV6 SYSTEM DASHBOARD          ");
  printf(ESC "[2;1H======================================");

  // Main Loop: Runs until "quit" file exists
  while(open("quit", 0) < 0) {
    if(ugetstats(&s) < 0) {
      printf("Stats Error\n");
      break;
    }

    uint64 used = TOTAL_MEM - s.freemem;

    // 1. Uptime
    printf(ESC "[3;1HSystem Uptime: %d s (%d ticks)    ", 
           s.uptime_ticks/10, s.uptime_ticks);

    // 2. Memory Bar
    printf(ESC "[5;1HMemory Used:   %d / %d MB    ", 
           (int)(used >> 20), (int)(TOTAL_MEM >> 20));
    draw_bar(6, used, TOTAL_MEM);

    // 3. Process Bar (New Feature)
    printf(ESC "[8;1HProcess Load:  %d / %d Slots    ", 
           s.total_procs, MAX_PROCS);
    draw_bar(9, s.total_procs, MAX_PROCS);

    // 4. Details
    printf(ESC "[11;1H [R] Running:  %d    ", s.n_running);
    printf(ESC "[12;1H [W] Runnable: %d    ", s.n_runnable);
    printf(ESC "[13;1H [S] Sleeping: %d    ", s.n_sleeping);
    printf(ESC "[14;1H [Z] Zombie:   %d    ", s.n_zombie);

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