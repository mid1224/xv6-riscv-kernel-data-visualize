#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

// A task that eats CPU and some Memory
void busy_task(int mem_mb) {
  // Allocate some memory
  if(mem_mb > 0) {
    sbrk(mem_mb * 1024 * 1024);
  }
  // Infinite CPU loop, never returns
  int x = 0;
  while(1) {
    x += 1;
    if(x % 10000000 == 0) {
      pause(1); // Yield CPU occasionally
    }
  }
}

// Spawn N children
void spawn_children(int n, int mem_per_child) {
  printf("Spawning %d children (%d MB each)...\n", n, mem_per_child);
  for(int i = 0; i < n; i++) {
    int pid = fork();
    if(pid == 0) {
      busy_task(mem_per_child);
      exit(0);
    } else if(pid < 0) {
      printf("Fork failed at child %d\n", i);
      break;
    }
  }
  // Just waits forever so children stay alive
  while(1) pause(100); 
}

int main(int argc, char *argv[])
{
  if(argc < 2){
    printf("Usage: spam [light | medium | heavy]\n"); // Instructions for user
    exit(0);
  }

  int pid = fork();
  if(pid < 0){
    printf("Fork failed\n");
    exit(1);
  }
  if(pid > 0){
    // Parent exits, shell continues
    pause(1);
    printf("Spam started in background (PID %d)\n", pid);
    exit(0);
  }

  // Spam presets
  if(strcmp(argv[1], "light") == 0){
    // LIGHT: 2 processes, 2MB RAM for each
    spawn_children(2, 2); 
  } 
  else if(strcmp(argv[1], "medium") == 0){
    // MEDIUM: 10 processes, 3MB RAM for each
    spawn_children(10, 3);
  } 
  else if(strcmp(argv[1], "heavy") == 0){
    // HEAVY: 45 processes, 2MB RAM for each
    spawn_children(45, 2);
  } 
  else {
    printf("Unknown command\n");
  }
  // Note: Xv6 limit is NPROC=64, don't go above this, also leave some free for other processes

  exit(0);
}