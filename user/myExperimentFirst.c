// first experiment---nineth operation
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char const *argv[])
{
    int count = getprocs();
    printf("Number of active processes: %d\n", count);
    exit(0);
}