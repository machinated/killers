#define _POSIX_C_SOURCE 199309L
#define _BSD_SOURCE
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include "message.h"
#include "messaging.h"
#include "agent.h"


/* Check whether the specified <time> already passed.
 * Return:
 *  '1' - True;
 *  '0' - 'False'
 */
int momentPassed(Timespec time)
{
    Timespec current;
    if(clock_gettime(CLOCK_REALTIME, &current))
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

/* Send notification to the company that the specified <killer> is ready. */
void SendKillerReady(int killer)
{
    MessageKillerReady msgK;
    msgK.killer = killer;
    Debug("Sending KILLER_READY to %d, killer: %d", processId, killer);
    Send(&msgK, processId, TAG_KILLER_READY);
}

/* Determine the minimal time value based on the given <a> and <b>.
 * Return the smaller one.
 */
Timespec MinTimer(Timespec a, Timespec b)
{
    if (a.tv_sec < b.tv_sec)
    {
        return a;
    }
    if (b.tv_sec < a.tv_sec)
    {
        return b;
    }
    if (a.tv_nsec < b.tv_nsec)
    {
        return a;
    }
    if (b.tv_nsec < a.tv_nsec)
    {
        return b;
    }
    return a;
}


/* Determine when the fastest killer will complete his job.
 * Note that only killers with status <K_BUSY> are considered.
 * Note that the <killersMutex> needs to be taken by a coller.
 */
Timespec GetSleepTime()
{
    Timespec minTimer;
    for (int killer = 0; killer < nKillers; killer++)
    {
        if (Killers[killer].status == K_BUSY)
        {
            minTimer = MinTimer(minTimer, Killers[killer].timer);
        }
    }
    return minTimer;
}


/* Agent of killers in the current company */
void* RunAgent(void* arg)
{
    while(1)
    {
        pthread_mutex_lock(&killersMutex);
        Timespec sleepTime = GetSleepTime();
        pthread_cond_timedwait(&wakeUpAgent, &killersMutex, &sleepTime);
        // killersMutex locked /* FIXME - is it really locked by who??? */

        /* Check all killers which have done their job and send notification to the manager in the company. */
        for (int killer = 0; killer < nKillers; killer++)
        {
            if (Killers[killer].status == K_BUSY &&
                momentPassed(Killers[killer].timer))
            {
                Debug("Killer %d is ready", killer);
                /* Send notification to the company that the i-th <killer> is already ready and his job/task is DONE.  */
                SendKillerReady(killer);
                Killers[killer].status = K_NOTIFICATION_SENT;
            }
        }
        /* FIXME where this mutex is taken? */
        pthread_mutex_unlock(&killersMutex);
    }
}
