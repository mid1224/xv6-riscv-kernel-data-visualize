#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

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

    exit(0);
}