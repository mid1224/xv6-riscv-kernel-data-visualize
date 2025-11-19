#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

#define CLEAR_SCREEN "\x1b[2J\x1b[H"
// ANSI Escape Codes
// \x1b[2J  = Clear entire screen
// \x1b[H   = Move cursor to top-left (Home)

int main(void)
{
    // the user side program lives here

    printf("\n================ kgetstats() result:\n");

    kgetstats();

    printf("user: kgetstats() returned, back in user mode\n");

    printf("\n================ ugetstats() result:\n");

    struct kstats stat; // Store the data from ugetstats()

    ugetstats(&stat);

    // Print the data we received from the kernel
    printf("user: Free Memory (in bytes): %ld\n", stat.freemem);
    printf("user: Free Memory (in megabytes): %ld\n", stat.freemem/(1024 * 1024)); //Converted from byte to megabyte for easier reading
    printf("user: Total Procs: %d\n", stat.total_procs);
    printf("user: Runnable: %d\n", stat.n_runnable);
    printf("user: Sleeping: %d\n", stat.n_sleeping);
    printf("user: Zombie: %d\n", stat.n_zombie);
    printf("user: Running: %d\n", stat.n_running);

    pause(20); // Temp delay for testing
    printf("\n================ Test realtime result:\n");
    pause(20);

    while(1) 
    {
        // Get the latest data
        if(ugetstats(&stat) < 0) {
            printf("Error getting stats\n");
            exit(1);
        }

        // Clear the screen so we can overwrite the old data
        printf(CLEAR_SCREEN);

        // Print the data
        printf("======================================\n");
        printf("      XV6 SYSTEM DASHBOARD            \n");
        printf("======================================\n");
        // Convert ticks to seconds (approx 10 ticks = 1 sec in QEMU)
        printf("System Uptime: %d seconds (%d ticks)\n", stat.uptime_ticks / 10, stat.uptime_ticks);
        printf("--------------------------------------\n");
        printf("Free Memory:   %ld bytes (%ld megabytes)\n", stat.freemem, stat.freemem/(1024 * 1024));
        printf("--------------------------------------\n");
        printf("PROCESS STATUS (Total: %d)\n", stat.total_procs);
        printf(" [R] Running:  %d\n", stat.n_running);
        printf(" [W] Runnable: %d\n", stat.n_runnable);
        printf(" [S] Sleeping: %d\n", stat.n_sleeping);
        printf(" [Z] Zombie:   %d\n", stat.n_zombie);
        printf("======================================\n");

        // Update Time Interval (20 ticks ~ 2 second)
        pause(20);
    }

    exit(0);
}