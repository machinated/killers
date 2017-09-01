#define _POSIX_C_SOURCE 199309L
#define _BSD_SOURCE
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "message.h"
#include "messaging.h"
#include "agent.h"

#define KILLTIME 4000

typedef struct timespec Timespec;
Timespec* Killers;

int parent;

int momentPassed(Timespec time)
{
    Timespec current;
    if(clock_gettime(CLOCK_MONOTONIC, &current))
    {
        Log("Error getting current time");
    }
    if (time.tv_sec < current.tv_sec)
    {
        return 1;
    }
    else if (current.tv_sec < time.tv_sec)
    {
        return 0;
    }
    else
    {
        if (time.tv_nsec <= current.tv_nsec)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
}

void AddTime(Timespec* timeP, unsigned long millis)
{
    timeP->tv_nsec += (millis - (millis/1000) * 1000) * 1000000;
    if (timeP->tv_nsec > 1000000000)
    {
        timeP->tv_nsec -= 1000000000;
        timeP->tv_sec += 1;
    }
    timeP->tv_sec += millis/1000;
}

void SetKillerTimer(int killer)
{
    Timespec timer;
    if(clock_gettime(CLOCK_MONOTONIC, &timer))
    {
        Log("Error getting current time");
    }

    int32_t randNumber;
    random_r(randState, &randNumber);
    int millis = randNumber % (KILLTIME * 2);

    AddTime(&timer, millis);
    Log("Timer for killer %d set for %d ms", killer, millis);
    Killers[killer] = timer;
}

void SendKillerReady(int killer)
{
    MessageKillerReady msgK;
    msgK.killer = killer;
    Log("Sending KILLER_READY to %d, killer: %d", parent, killer);
    Send(&msgK, parent, TAG_KILLER_READY);
}

void OnKillerReady(int killer)
{
    SendKillerReady(killer);
    if (AwaitAck(parent) == ACK_OK)
    {
        SetKillerTimer(killer);
    }
    else                // ACK_REJECT
    {
        // killers[killer].tv_sec = 0;
        // killers[killer].tv_nsec = 0;

        Log("Killer %d rejected", killer);
        Killers[killer].tv_sec += 1;
    }
}

void RunAgent()
{
    parent = processId - nCompanies;
    Log("Process %d running as agent for %d", processId, parent);
    Killers = (Timespec*) calloc(nKillers, sizeof(Timespec));

    for (int killer = 0; killer < nKillers; killer++)
    {
        Timespec current;
        if(clock_gettime(CLOCK_MONOTONIC, &current))
        {
            Log("Error getting current time");
        }
        Killers[killer] = current;
    }

    while(1)
    {
        for (int killer = 0; killer < nKillers; killer++)
        {
            if (momentPassed(Killers[killer]))
            {
                Log("Killer %d is ready", killer);
                OnKillerReady(killer);
            }
        }
        milisleep(1);
    }
}
