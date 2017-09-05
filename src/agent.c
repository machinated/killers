#define _POSIX_C_SOURCE 199309L
#define _BSD_SOURCE
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include "message.h"
#include "messaging.h"
#include "agent.h"

int momentPassed(Timespec time)
{
    Timespec current;
    if(clock_gettime(CLOCK_MONOTONIC, &current))
    {
        Error("Error getting current time");
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

void SendKillerReady(int killer)
{
    MessageKillerReady msgK;
    msgK.killer = killer;
    Debug("Sending KILLER_READY to %d, killer: %d", processId, killer);
    Send(&msgK, processId, TAG_KILLER_READY);
}

// Timespec MinTimer(Timespec a, Timespec b)
// {
//     if (a.tv_sec < b.tv_sec)
//     {
//         return a;
//     }
//     if (b.tv_sec < a.tv_sec)
//     {
//         return b;
//     }
//     if (a.tv_nsec < b.tv_nsec)
//     {
//         return a;
//     }
//     if (b.tv_nsec < a.tv_nsec)
//     {
//         return b;
//     }
//     return a;
// }
//
// Timespec GetSleepTime()
// {
//     Timespec minTimer;
//     pthread_mutex_lock(&killersMutex);
//     for (int killer = 0; killer < nKillers; killer++)
//     {
//         if (Killers[killer].client != -1)
//         {
//             minTimer = MinTimer(minTimer, Killers[killer].timer);
//         }
//     }
//     pthread_mutex_unlock(&killersMutex);
// }

void* RunAgent(void* arg)
{
    while(1)
    {
        pthread_mutex_lock(&killersMutex);
        for (int killer = 0; killer < nKillers; killer++)
        {
            if (Killers[killer].client != -1 &&
                momentPassed(Killers[killer].timer))
            {
                Debug("Killer %d is ready", killer);
                SendKillerReady(killer);
            }
        }
        pthread_mutex_unlock(&killersMutex);
        milisleep(10);
    }
}
