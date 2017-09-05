#define _BSD_SOURCE
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "messaging.h"
#include "client.h"
#include "company.h"
#include "agent.h"

const char helpMsg[] =
"USAGE: killers -c N -k Z [-s S -t T -l L]\n"
"OPTIONS:\n"
"   -c N        number of companies\n"
"   -k Z        number of killers\n"
"   -s S        clients wait for S ms (default 3000)\n"
"   -t T        time it takes for killer to kill someone in ms (default 4000)\n"
"   -l L        print messages with L or higher priority (default 2)\n"
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
    // MPI_Init(&argc, &argv);
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

    // init RNG
    srand(processId);

    if (processId < nCompanies)
    {
        RunCompany();
    }
    else if (processId < nCompanies * 2)
    {
        RunAgent();
    }
    else
    {
        RunClient();
    }

    MPI_Finalize();
}
