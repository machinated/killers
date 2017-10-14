// #define _BSD_SOURCE
#define _POSIX_C_SOURCE 199309L
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "messaging.h"
#include "logging.h"
#include "common.h"
#include "client.h"

const char helpMsg[] =
"USAGE: killers -c N -k Z [-s S -t T -l L]\n"
"OPTIONS:\n"
"   -c N        Number of companies\n"
"   -k Z        Number of killers\n"
"   -s S        Clients wait for S ms (default 3000 ms)\n"
"   -t T        Time in ms needed for a single killer to kill someone (default 4000 ms)\n"
"   -l L        Print messages with the specifed priority level L or higher (default 2), where:\n"
"               0 = TRACE, 1 = DEBUG, 2 = INFO, 3 = WARNING, 4 = ERROR\n";

void parseOptions(int argc, char* argv[])
{
    SLEEPTIME = 3000;
    KILLTIME = 4000;
    logPriority = LOG_INFO;
    int option;
    while(1)
    {
        option = getopt(argc, argv, "hc:k:s:t:l:");
        if (option == -1)
        {
            break;
        }
        switch (option)
        {
            case 'h':
            {
                if (processId == 0)
                {
                    printf(helpMsg);
                }
                MPI_Abort(MPI_COMM_WORLD, 0);
            }
            case 'c':
            {
                nCompanies = atoi(optarg);
                break;
            }
            case 'k':
            {
                nKillers = atoi(optarg);
                break;
            }
            case 's':
            {
                SLEEPTIME = atoi(optarg);
                break;
            }
            case 't':
            {
                KILLTIME = atoi(optarg);
                break;
            }
            case 'l':
            {
                logPriority = atoi(optarg);
                break;
            }
        }
    }
    if (processId == 0)
    {
        printf("nCompanies = %d, nKillers = %d\n", nCompanies, nKillers);
        if (nCompanies < 0 || nKillers < 0 || nCompanies * 2 > nProcesses)
        {
            printf("Invalid arguments");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        if (logPriority < 0 || logPriority > 5)
        {
            printf("Invalid log level: %d", logPriority);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        #ifdef DEBUG
        printf("DEBUG\n");
        #endif
        #ifdef NDEBUG
        printf("NDEBUG\n");
        #endif
    }
}

int main(int argc, char* argv[])
{

    int provided;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE)
    {
        printf("ERROR: The MPI library does not have full thread support\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    localClock = 0;

    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

    parseOptions(argc, argv);

    srand(processId * 13);

    RunClient();

    MPI_Finalize();
}
