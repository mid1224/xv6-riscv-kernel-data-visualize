#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main(void)
{
    // the user side program lives here

    // call kernel side "program"
    program();

    printf("user: program() returned, back in user mode\n");
    exit(0);
}