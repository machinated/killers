#ifndef _COMMON
#define _COMMON
#include <stdint.h>
#include <mpi.h>

#define NO_CLIENT MPI_UNDEFINED

uint64_t localClock;

int processId;     /* ID of the current process. */

int nProcesses;    /* The total number of processes */

int nCompanies;    /* The number of processes assigned to companies */

int nKillers;      /* The number of killers per a company. */

int SLEEPTIME;     /* How long a customer thinks about a new task/job (max value). */

int KILLTIME;      /* How much time a killer need to complete his task/job (max period of time). */

#endif
