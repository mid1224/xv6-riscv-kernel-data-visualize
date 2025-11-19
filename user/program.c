#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fcntl.h"

#define CLEAR_SCREEN "\x1b[2J\x1b[H"
// ANSI Escape Codes
// \x1b[2J  = Clear entire screen
// \x1b[H   = Move cursor to top-left (Home)

static void restore_and_exit(int code) {
    // clear the screen, show cursor, then exit
    printf(CLEAR_SCREEN);
    printf("\x1b[?25h");
    exit(code);
}

int main(void)
{
    // the user side program lives here

    printf("\n================ kgetstats() result:\n");

    kgetstats();

    printf("user: kgetstats() returned, back in user mode\n");

    printf("\n================ ugetstats() result:\n");

    struct kstats stat; // Store the data from ugetstats()

    // initial read so we have a "previous" snapshot
    struct kstats prev;
    if (ugetstats(&prev) < 0) {
        printf("Error getting stats\n");
        restore_and_exit(1);
    }

    // Draw header once
    printf(CLEAR_SCREEN);
    printf("\x1b[?25l"); // Hide cursor
    printf("======================================\n");
    printf("      XV6 SYSTEM DASHBOARD            \n");
    printf("======================================\n");
    printf("System Uptime: %d seconds (%d ticks)\n", prev.uptime_ticks / 10, prev.uptime_ticks);
    printf("--------------------------------------\n");
    printf("Free Memory:   %ld bytes (%ld megabytes)\n", prev.freemem, prev.freemem / (1024 * 1024));
    printf("--------------------------------------\n");
    printf("PROCESS STATUS (Total: %d)\n", prev.total_procs);
    printf(" [R] Running:  %d\n", prev.n_running);
    printf(" [W] Runnable: %d\n", prev.n_runnable);
    printf(" [S] Sleeping: %d\n", prev.n_sleeping);
    printf(" [Z] Zombie:   %d\n", prev.n_zombie);
    printf("======================================\n");

    // spawn watcher: creates "quit" file when user types 'x' or 'X'
    if (fork() == 0) {
        char ch;
        while (read(0, &ch, 1) > 0) {
            if (ch == 'x' || ch == 'X') {
                int fd = open("quit", O_CREATE | O_WRONLY);
                if (fd >= 0) {
                    write(fd, "x", 1);
                    close(fd);
                }
                exit(0);
            }
        }
        exit(0);
    }

    while (1)
    {
        if (ugetstats(&stat) < 0) {
            printf("Error getting stats\n");
            restore_and_exit(1);
        }

        // Only update lines that changed.
        // Line numbers correspond to the header printed above:
        // 4 = System Uptime
        // 6 = Free Memory
        // 8 = PROCESS STATUS (total)
        // 9 = Running
        // 10 = Runnable
        // 11 = Sleeping
        // 12 = Zombie

        //plan to use switch case but messier than ifs

        if (stat.uptime_ticks != prev.uptime_ticks) {
            // move to line 4, column 1 and overwrite (pad spaces to clear leftovers)
            printf("\x1b[4;1HSystem Uptime: %d seconds (%d ticks)          \n", stat.uptime_ticks / 10, stat.uptime_ticks);
        }

        if (stat.freemem != prev.freemem) {
            printf("\x1b[6;1HFree Memory:   %ld bytes (%ld megabytes)          \n", stat.freemem, stat.freemem / (1024 * 1024));
        }

        if (stat.total_procs != prev.total_procs) {
            printf("\x1b[8;1HPROCESS STATUS (Total: %d)                  \n", stat.total_procs);
        }

        if (stat.n_running != prev.n_running) {
            printf("\x1b[9;1H [R] Running:  %d                    \n", stat.n_running);
        }

        if (stat.n_runnable != prev.n_runnable) {
            printf("\x1b[10;1H [W] Runnable: %d                    \n", stat.n_runnable);
        }

        if (stat.n_sleeping != prev.n_sleeping) {
            printf("\x1b[11;1H [S] Sleeping: %d                    \n", stat.n_sleeping);
        }

        if (stat.n_zombie != prev.n_zombie) {
            printf("\x1b[12;1H [Z] Zombie:   %d                    \n", stat.n_zombie);
        }

        // update rate
        pause(1);

        // check if watcher requested quit
        int qfd = open("quit", 0);
        if (qfd >= 0) {
            close(qfd);
            unlink("quit");   // remove sentinel
            restore_and_exit(0);
        }

        prev = stat;
    }

    restore_and_exit(0);
}