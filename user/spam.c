#include "user.h"
#include "kernel/fcntl.h"

/* pid-file helpers (unchanged) */
static void write_pid_file(int pid) {
    int fd = open("spam.pid", O_CREATE | O_WRONLY);
    if (fd < 0) return;
    char buf[16];
    int n = 0;
    if (pid == 0) { buf[n++] = '0'; }
    else {
        int x = pid;
        char rev[16]; int r = 0;
        while (x > 0 && r < (int)sizeof(rev)) { rev[r++] = '0' + (x % 10); x /= 10; }
        while (r > 0) buf[n++] = rev[--r];
    }
    buf[n++] = '\n';
    write(fd, buf, n);
    close(fd);
}

static int read_pid_file(void) {
    int fd = open("spam.pid", 0);
    if (fd < 0) return -1;
    char buf[16];
    int n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return atoi(buf);
}

static void remove_pid_file(void) { unlink("spam.pid"); }

static void request_quit(void) {
    int fd = open("quit", O_CREATE | O_WRONLY);
    if (fd >= 0) {
        write(fd, "x", 1);
        close(fd);
    }
    exit(0);
}

/* usage */
static void usage(void) {
    printf("usage:\n");
    printf("  spam -q\n");
    printf("  spam alloc [blocks] [size]\n");
    printf("  spam fork  [n]\n");
    printf("  spam zombie [n]\n");
    printf("  spam sleep [n] [dur]\n");
    printf("  spam busy\n");
    printf("  spam all\n");
}

/* simple LCG PRNG per-worker (deterministic, cheap) */
static unsigned int rng_next(unsigned int *s) {
    *s = (*s * 1664525u) + 1013904223u;
    return *s;
}

/* safer bounded malloc spam: allocate up to max_blocks, then free periodically */
static void malloc_spam(int blocks, int size) {
    int i = 0;
    int max_blocks = blocks > 0 ? blocks : 20; // never allocate more than 20 blocks by default
    char *bufs[32];
    int bufs_count = 0;

    for (;;) {
        if (i >= max_blocks) break;
        char *p = sbrk(size);
        if (p == (char*)-1) {
            printf("sbrk failed after %d allocations\n", i);
            break;
        }
        p[0] = 1;
        if (bufs_count < (int)(sizeof(bufs)/sizeof(bufs[0]))) {
            bufs[bufs_count++] = p;
        }
        i++;
        pause(1);
    }

    /* free what we allocated, best-effort (sbrk negative) */
    for (int j = bufs_count - 1; j >= 0; j--) {
        sbrk(-size);
    }
    exit(0);
}

/* fork_spam: create n busy children (bounded) */
static void fork_spam(int n) {
    if (n > 20) n = 20;
    for (int i = 0; i < n; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("fork failed at %d\n", i);
            break;
        }
        if (pid == 0) {
            volatile int x = 0;
            while (1) {
                for (int j = 0; j < 50000; j++) x += j;
                pause(1);
            }
            exit(0);
        }
    }
    pause(100000);
    exit(0);
}

/* zombie_spam: create short-lived children but reap them so table won't fill */
static void zombie_spam(int n) {
    if (n > 20) n = 20;
    for (int i = 0; i < n; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("fork failed at %d\n", i);
            break;
        }
        if (pid == 0) exit(0);
        pause(1);
        {
            int st;
            wait(&st);
        }
    }
    pause(100000);
    exit(0);
}

/* sleeper_spam: spawn n sleepers */
static void sleeper_spam(int n, int dur) {
    if (n > 20) n = 20;
    for (int i = 0; i < n; i++) {
        int pid = fork();
        if (pid < 0) break;
        if (pid == 0) {
            pause(dur);
            exit(0);
        }
    }
    pause(100000);
    exit(0);
}

/* busy single */
static void busy_single(void) {
    volatile int x = 0;
    while (1) {
        for (int i = 0; i < 100000; i++) x += i;
        pause(1);
    }
    exit(0);
}

/* worker: randomized behavior (bounded, cycles through actions) */
static void worker_loop(int id) {
    unsigned int state = (unsigned int)(getpid() ^ id ^ 0xA5A5A5A5u);
    const int MAX_LOCAL_ALLOCS = 6;
    int alloc_sizes[MAX_LOCAL_ALLOCS];
    int alloc_count = 0;

    for (int iter = 0; iter < 10000; iter++) {
        unsigned int r = rng_next(&state);
        int action = r % 4;
        if (action == 0) {
            /* busy */
            int work = 10000 + (rng_next(&state) % 50000);
            volatile int x = 0;
            for (int i = 0; i < work; i++) x += i;
            pause(1 + (rng_next(&state) % 3));
        } else if (action == 1) {
            /* sleep */
            int dur = 1 + (rng_next(&state) % 10);
            pause(dur);
        } else if (action == 2) {
            /* small alloc + free (bounded) */
            if (alloc_count < MAX_LOCAL_ALLOCS) {
                int sz = 1024 * (1 + (rng_next(&state) % 8)); /* 1..8 KB */
                char *p = sbrk(sz);
                if (p != (char*)-1) {
                    p[0] = 1;
                    alloc_sizes[alloc_count++] = sz;
                }
            } else {
                /* free oldest */
                int old = alloc_sizes[0];
                /* shift left */
                for (int j = 1; j < alloc_count; j++) alloc_sizes[j-1] = alloc_sizes[j];
                alloc_count--;
                /* best-effort free */
                sbrk(-old);
            }
            pause(1);
        } else {
            /* yield / light sleep */
            pause(1 + (rng_next(&state) % 2));
        }
    }

    /* cleanup any remaining local allocs */
    for (int j = alloc_count - 1; j >= 0; j--) sbrk(-alloc_sizes[j]);
    exit(0);
}

/* helper: try to fork, return child pid or -1; on failure, print and stop */
static int safe_fork_and_report(int i) {
    int pid = fork();
    if (pid < 0) {
        printf("fork failed at %d, stopping further forks\n", i);
    }
    return pid;
}

/* hog: allocate repeatedly without freeing until sbrk fails */
static void hog_spam(int size) {
    int cnt = 0;
    for (;;) {
        char *p = sbrk(size);
        if (p == (char*)-1) {
            printf("hog: sbrk failed after %d allocations\n", cnt);
            break;
        }
        p[0] = 1;   // touch page so physical memory is used
        cnt++;
        pause(1);   // let dashboard update
    }
    exit(0);
}

/* forward-declare helper used below to avoid implicit-declaration / linkage issues */
static void create_zombies_no_reap(int n, int spacing);

/* Gradual allocator: allocate 'blocks' of 'size' bytes, one every 'interval' ticks.
   Keeps allocations (doesn't free) so free-memory slowly decreases. */
static void gradual_malloc_spam(int blocks, int size, int interval) {
    int i = 0;
    for (;;) {
        if (i >= blocks) break;
        char *p = sbrk(size);
        if (p == (char*)-1) {
            printf("gradual: sbrk failed after %d allocations\n", i);
            break;
        }
        p[0] = 1;         // touch memory
        i++;
        pause(interval);  // wait before next allocation so change is visible
    }
    // stay alive so allocations persist and dashboard shows reduced free memory
    pause(200000);
    exit(0);
}

/* gradual ramp for "all" command: spawn actors slowly so metrics rise over time */
static void gradual_all(int intensity) {
    /* intensity:
       0 = safe slow ramp
       1 = medium ramp (more allocs/forks)
       2 = aggressive ramp (bigger allocs, more forks) */
    int steps_between = 10; /* ticks to wait between spawns */

    if (intensity == 0) {
        /* safe: a few workers and a slow bounded allocator */
        for (int i = 0; i < 4; i++) {
            int p = safe_fork_and_report(i);
            if (p == 0) worker_loop(i);   /* randomized worker */
            pause(steps_between);
        }
        /* one slow allocator: 10 blocks of 16KB, 5 ticks between each */
        if (safe_fork_and_report(10) == 0) gradual_malloc_spam(10, 16*1024, 5);
        pause(200000);
        remove_pid_file();
        exit(0);
    }

    if (intensity == 1) {
        /* medium: staggered allocators, a few busy children and sleepers */
        for (int i = 0; i < 3; i++) {
            if (safe_fork_and_report(100 + i) == 0)
                gradual_malloc_spam(12, 64*1024, 4); /* each allocates 12x64KB slowly */
            pause(steps_between);
        }
        if (safe_fork_and_report(200) == 0) fork_spam(6);    /* some busy children */
        pause(steps_between);
        if (safe_fork_and_report(201) == 0) sleeper_spam(6, 8000);
        pause(200000);
        remove_pid_file();
        exit(0);
    }

    /* intensity == 2 (aggressive): larger, but still staggered to show gradual rise) */
    if (intensity == 2) {
        /* spawn several gradual hoggers that allocate bigger blocks slowly */
        for (int i = 0; i < 3; i++) {
            if (safe_fork_and_report(300 + i) == 0)
                gradual_malloc_spam(30, 128*1024, 3); /* 30x128KB, slower ramp */
            pause(steps_between);
        }
        /* add some busy processes too, spaced out */
        for (int i = 0; i < 6; i++) {
            if (safe_fork_and_report(400 + i) == 0) {
                volatile int x = 0;
                while (1) { for (int j = 0; j < 100000; j++) x += j; pause(1); }
                exit(0);
            }
            pause(5);
        }

        /* create a few zombies (no reaping) so dashboard shows Zombie count rising */
        create_zombies_no_reap(6, 2);

        pause(200000);
        remove_pid_file();
        exit(0);
    }
}

/* create N children that exit immediately and are NOT reaped by parent
   (they become zombies until parent waits or dies). spacing in ticks. */
static void create_zombies_no_reap(int n, int spacing) {
    if (n > 20) n = 20;
    for (int i = 0; i < n; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("zombie fork failed at %d\n", i);
            break;
        }
        if (pid == 0) {
            // child exits immediately -> becomes a zombie
            exit(0);
        }
        // parent intentionally does NOT wait() here; space out creations
        pause(spacing);
    }
    // return to caller so caller can continue running (and keep zombies visible)
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "-q") == 0) {
        request_quit();
    }
    if (argc >= 2 && strcmp(argv[1], "-k") == 0) {
        int pid = read_pid_file();
        if (pid > 0) {
            if (kill(pid) < 0) printf("kill failed\n");
            else remove_pid_file();
        } else {
            printf("no spam.pid found\n");
        }
        exit(0);
    }

    if (argc < 2) {
        usage();
        exit(0);
    }

    /* background ourselves so the shell returns immediately */
    int pid = fork();
    if (pid < 0) {
        printf("failed to background\n");
    } else if (pid > 0) {
        /* parent exits so child continues in background */
        exit(0);
    }
    /* child continues here (background worker) */
    write_pid_file(getpid());

    if (strcmp(argv[1], "alloc") == 0) {
        int blocks = argc > 2 ? atoi(argv[2]) : 10;
        int size   = argc > 3 ? atoi(argv[3]) : (4 * 1024);
        malloc_spam(blocks, size);
    } else if (strcmp(argv[1], "fork") == 0) {
        int n = argc > 2 ? atoi(argv[2]) : 6;
        fork_spam(n);
    } else if (strcmp(argv[1], "zombie") == 0) {
        int n = argc > 2 ? atoi(argv[2]) : 4;
        zombie_spam(n);
    } else if (strcmp(argv[1], "sleep") == 0) {
        int n = argc > 2 ? atoi(argv[2]) : 4;
        int dur = argc > 3 ? atoi(argv[3]) : 10000;
        sleeper_spam(n, dur);
    } else if (strcmp(argv[1], "busy") == 0) {
        busy_single();
    } else if (strcmp(argv[1], "all") == 0) {
        int intensity = argc > 2 ? atoi(argv[2]) : 0;
        if (intensity < 0) intensity = 0;
        if (intensity > 2) intensity = 2;
        /* backgrounding already done earlier; call gradual_all to perform ramped spawns */
        gradual_all(intensity);
    } else if (strcmp(argv[1], "hog") == 0) {
        int size = argc > 2 ? atoi(argv[2]) : 200000; // bytes per alloc
        hog_spam(size);
    } else {
        usage();
        exit(0);
    }

    return 0;
}