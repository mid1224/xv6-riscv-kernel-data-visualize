#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fcntl.h"

/* Hardcoded total RAM for the memory bar (adjust to your QEMU -m value) */
#define TOTAL_MEM (128ULL * 1024 * 1024) /* 128 MB */

#define CLEAR_SCREEN "\x1b[2J\x1b[H"
#define HIDE_CURSOR  "\x1b[?25l"
#define SHOW_CURSOR  "\x1b[?25h"

/* convenience: clear/show and exit cleanly */
static void restore_and_exit(int code) {
    printf(CLEAR_SCREEN);
    printf(SHOW_CURSOR);
    exit(code);
}

/* draw a memory bar with spaced bars: "[ | | |   ... ] 12% usage"
   slots = number of 'bar' positions (each separated by one space) */
static void draw_mem_bar(uint64 freemem, uint64 totalmem, int slots) {
    int filled = 0;
    if (totalmem > 0) {
        uint64 used = (totalmem > freemem) ? (totalmem - freemem) : 0;
        filled = (int)((used * (uint64)slots) / totalmem);
        if (filled > slots) filled = slots;
    }
    /* Free Memory line (bytes + MB) at line 6 */
    printf("\x1b[6;1HFree Memory:   %ld bytes (%ld megabytes)          \n",
           (long)freemem, (long)(freemem / (1024 * 1024)));

    /* Bar line at line 7 — use block characters, no spaces */
    printf("\x1b[7;1H[");
    for (int i = 0; i < slots; i++) {
        if (i < filled) printf("█"); /* full block for used */
        else printf("░");             /* light shade for free */
    }
    /* percent usage = (used/total)*100 */
    int pct = 0;
    if (totalmem > 0) {
        uint64 used = (totalmem > freemem) ? (totalmem - freemem) : 0;
        pct = (int)((used * 100) / totalmem) ;
    }
    /* print number with printf (use plain %d) then write a literal '%' */
    printf("] %d", pct);
    write(1, "% usage          \n", sizeof("% usage          \n") - 1);
}

/* draw the static header once using prev snapshot */
static void draw_header(const struct kstats *prev) {
    printf(CLEAR_SCREEN);
    printf(HIDE_CURSOR);
    printf("======================================\n");
    printf("      XV6 SYSTEM DASHBOARD            \n");
    printf("======================================\n");
    /* Uptime line (line 4) */
    printf("System Uptime: %d seconds (%d ticks)          \n", prev->uptime_ticks / 10, prev->uptime_ticks);
    printf("--------------------------------------\n");
    /* line 6 and 7 will be drawn by draw_mem_bar */
    draw_mem_bar(prev->freemem, TOTAL_MEM, 20);
    printf("--------------------------------------\n");
    printf("PROCESS STATUS (Total: %d)\n", prev->total_procs);
    printf(" [R] Running:  %d\n", prev->n_running);
    printf(" [W] Runnable: %d\n", prev->n_runnable);
    printf(" [S] Sleeping: %d\n", prev->n_sleeping);
    printf(" [Z] Zombie:   %d\n", prev->n_zombie);
    printf("======================================\n");
}

int main(void)
{
    struct kstats stat; // latest snapshot
    struct kstats prev; // previous snapshot

    printf("\n================ kgetstats() result:\n");
    kgetstats();
    printf("user: kgetstats() returned, back in user mode\n");
    printf("\n================ ugetstats() result:\n");

    if (ugetstats(&prev) < 0) {
        printf("Error getting stats\n");
        restore_and_exit(1);
    }

    /* print header once */
    draw_header(&prev);

    /* spawn watcher: pressing Enter (empty line) creates "quit" sentinel */
    if (fork() == 0) {
        char ch;
        char buf[64];
        int idx = 0;
        while (read(0, &ch, 1) > 0) {
            if (ch == '\r') continue;
            if (ch == '\n') {
                if (idx == 0) {
                    int fd = open("quit", O_CREATE | O_WRONLY);
                    if (fd >= 0) { write(fd, "x", 1); close(fd); }
                    exit(0);
                }
                idx = 0;
                continue;
            }
            if (idx < (int)sizeof(buf) - 1) buf[idx++] = ch;
            /* quick quit on 'x' anywhere */
            if (ch == 'x' || ch == 'X') {
                int fd = open("quit", O_CREATE | O_WRONLY);
                if (fd >= 0) { write(fd, "x", 1); close(fd); }
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

        /* update uptime (line 4) */
        if (stat.uptime_ticks != prev.uptime_ticks) {
            printf("\x1b[4;1HSystem Uptime: %d seconds (%d ticks)          \n",
                   stat.uptime_ticks / 10, stat.uptime_ticks);
        }

        /* update memory lines (6 + bar on 7) */
        if (stat.freemem != prev.freemem) {
            draw_mem_bar(stat.freemem, TOTAL_MEM, 20);
        }

        /* process summary and per-state lines */
        if (stat.total_procs != prev.total_procs) {
            printf("\x1b[9;1HPROCESS STATUS (Total: %d)                  \n", stat.total_procs);
        }
        if (stat.n_running != prev.n_running) {
            printf("\x1b[10;1H [R] Running:  %d                    \n", stat.n_running);
        }
        if (stat.n_runnable != prev.n_runnable) {
            printf("\x1b[11;1H [W] Runnable: %d                    \n", stat.n_runnable);
        }
        if (stat.n_sleeping != prev.n_sleeping) {
            printf("\x1b[12;1H [S] Sleeping: %d                    \n", stat.n_sleeping);
        }
        if (stat.n_zombie != prev.n_zombie) {
            printf("\x1b[13;1H [Z] Zombie:   %d                    \n", stat.n_zombie);
        }

        /* keep update cadence */
        pause(1);

        /* check sentinel */
        int qfd = open("quit", 0);
        if (qfd >= 0) {
            close(qfd);
            unlink("quit");
            restore_and_exit(0);
        }

        prev = stat;
    }

    restore_and_exit(0);
}