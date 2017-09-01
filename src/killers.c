#define _BSD_SOURCE
#include <mpi.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "messaging.h"
#include "client.h"
#include "company.h"

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    localClock = 0;

    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

    nCompanies = 2;
    nKillers = 1;
    // TODO cmd line arguments

    assert(nCompanies < nProcesses);

    // init RNG
    char* bytestate = malloc(128);
    randState = malloc(sizeof(struct random_data));
    initstate_r(processId, bytestate, 128, randState);

    if (processId < nCompanies)
    {
        RunCompany();
    }
    else
    {
        RunClient();
    }

    MPI_Finalize();
}
