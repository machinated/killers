#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <mpi.h>
#include <errno.h>
#include "utils.h"

/* This routine allows to give back a CPU for at least the specified amount of time.
  * It can be used during waiting for some action. */
void milisleep(long ms)
{
    struct timespec sleepTime;
    sleepTime.tv_sec =  (time_t)(ms / 1000);
    sleepTime.tv_nsec = (long)((ms - (sleepTime.tv_sec * 1000)) * 1000);
    if (nanosleep(&sleepTime, NULL) != 0)
    {
        printf("Nanosleep error: %d\n", errno);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}
