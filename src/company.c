#define _POSIX_C_SOURCE 199309L
#define _BSD_SOURCE
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <pthread.h>
#include <mpi.h>
#include "message.h"
#include "messaging.h"
#include "agent.h"
#include "company.h"

/* The Queue[customers]  */
int* Queue;     /* Queue[queuePos] is a client's process number */
/* Length of the current queue with customers */
int queueLen;
/* We need another thread for the agent of killers */
pthread_t agentThread;


/* Send the TAG_UPDATE message to the <dest> customer with the assigned
 * <queuePos> position in the queue.
 */
void SendUpdate(int queuePos, int dest)
{
    MessageUpdate msgUpdate;
    msgUpdate.queueIndex = queuePos;
    Debug("Sending UPDATE to %d, place in queue: %d", dest, queuePos);
    Send(&msgUpdate, dest, TAG_UPDATE);
}

/* This routine allows printing of the current queue with customers. */
void PrintQueue()
{
    char* str = (char*) calloc(queueLen * 5, sizeof(char));
    char elem[5];
    for (int qi = 0; qi < queueLen; qi++)
    {
        sprintf(elem, "%d ", Queue[qi]);
        strncat(str, elem, 5);
    }
    Debug("QUEUE: %s", str);
    free(str);
}

/* This routine checks whether the specified customer is already in the queue.
 * Returns:
 * 1 - when found
 * 0 - when NOT found
 */
int QueueContains(int client)
{
    for (int i = 0; i < queueLen; i++)
    {
        if (Queue[i] == client)
        {
            return 1;
        }
    }
    return 0;
}


/* Add the specified <client> to the queue. */
void QueueAdd(int client)
{
    assert(!QueueContains(client));
    Queue[queueLen] = client;
    queueLen++;
    assert(queueLen <= nProcesses);
    SendUpdate(queueLen - 1, client);
    assert(queueLen <= nProcesses);
    Debug("Added %d to queue", client);
    PrintQueue();
}

/* Remove the specified <client> from the queue. */
void QueueRemove(int client)
{
    int found = 0;
    int qi;
    for (qi = 0; qi < queueLen; qi++)
    {
        if (Queue[qi] == client)
        {
            found = 1;
            break;
        }
    }
    if (found)
    {
        for( ; qi < queueLen - 1; qi++)
        {
            Queue[qi] = Queue[qi+1];
            SendUpdate(qi, client);
        }
        queueLen--;
        Debug("Removed %d from queue", client);
        PrintQueue();
    }
    else
    {
        Error("Attempting to remove %d, but not in queue", client);
    }
}

/* Take the first element from the queue (if any) */
int QueuePop()
{
    if (queueLen < 1)
    {
        Debug("QueuePop: no elements");
        return NO_CLIENT;
    }
    int first = Queue[0];
    for (int qi = 0; qi < queueLen - 1; qi++)
    {
        Queue[qi] = Queue[qi+1];
        SendUpdate(qi, Queue[qi]);
    }
    queueLen--;
    Debug("Popped %d from queue", first);
    PrintQueue();
    return first;
}

/* Add the specified number of milliseconds <millis> to the given <timeP>. */
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

/* Set the timer of the specified <killer> */
void SetKillerTimer(int killer)
{
    Timespec timer;
    if(clock_gettime(CLOCK_REALTIME, &timer))
    {
        Error("Error getting current time");
    }

    int millis = rand() % (KILLTIME * 2);

    AddTime(&timer, millis);
    Debug("Timer for killer %d set for %d ms", killer, millis);
    Killers[killer].timer = timer;
    /* Wakeup the agent */
    pthread_cond_signal(&wakeUpAgent);
}

/* Assign a new job/task to the specified <killer> */
void NewJob(int killer)
{
    pthread_mutex_lock(&killersMutex);
    int oldClient = Killers[killer].client;
    /* If there was a previous job, it has been DONE already.
     * Send a notification to the customer. */
    if (oldClient >= 0)
    {
        SendUpdate(Q_DONE, oldClient);
        Killers[killer].client = NO_CLIENT;
        Killers[killer].status = K_READY;
    }

    /* If there is any pending job in the queue with customers. */
    if (queueLen > 0)
    {
        int client = QueuePop();
        while (client != NO_CLIENT)
        {
            /* Send notification that the job can be now assigned to a killer */
            SendUpdate(Q_IN_PROGRESS, client);
            /* Get the job done after confirmation by the <client>. */
            if (AwaitAck(client) == ACK_OK)
            {
                break;
            }
            /* Take the next job from the list. */
            client = QueuePop();
        }
        /* Update a customer for the current killer. (-1) for unknown customer. */
        Killers[killer].client = client;
        /* If we have a customer with a job for this killer:
         * set time of this task and update the killer's status */
        if (client >= 0)
        {
            SetKillerTimer(killer);
            Killers[killer].status = K_BUSY;
        }
    }
    else         /* FIXME - do we need this? In which case? */
    {
        Killers[killer].client = NO_CLIENT;
        Killers[killer].status = K_READY;
    }
    pthread_mutex_unlock(&killersMutex);
}

/* Try to find a free killer.
 * Returns:
 * - '-1' when not found;
 * - Otherwise, id of the found killer.
 */

int GetFreeKiller()
{
    pthread_mutex_lock(&killersMutex);

    int killer = NO_FREE_KILLER;
    for (int ik = 0; ik < nKillers; ik++)
    {
        if (Killers[ik].status != K_BUSY)
        {
            killer = ik;
            break;
        }
    }
    pthread_mutex_unlock(&killersMutex);
    return killer;
}

/* Obtain any message and handle it */
void ReceiveMessages()
{
    Message msg;
    int tag, sender;
    ReceiveAny(&msg, &tag, &sender);

    switch(tag)
    {
        /* This is a request from a customer. */
        case TAG_REQUEST:
        {
            /* Append it to the queue with pending customers. */
            QueueAdd(sender);
            if (queueLen == 1)
            {
                int killer = GetFreeKiller();
                /* If there is a free killer witout job, assign it. */
                if (killer != NO_FREE_KILLER)
                {
                    NewJob(killer);
                }
            }
            break;
        }
        /* A customer <sender> has cancelled a reservation in a queue. */
        case TAG_CANCEL:
        {
            /* Job cancelled. That's OK, just remove this job/customer from the queue with pending tasks. */
            QueueRemove(sender);
            break;
        }
        /* Notification from the agent that a killer is ready. */
        case TAG_KILLER_READY:
        {
            MessageKillerReady* msgK = (MessageKillerReady*) &(msg.data.data);
            int killer = msgK->killer;
            Debug("Received KILLER_READY from %d, killer: %d", sender, killer);
            /* Try to reassign a next job for this killer. */
            NewJob(killer);
            break;
        }
        default:
        {
            Debug("Received message with invalid tag: %d", tag);
        }
    }
}

/* Run a new company */
void RunCompany()
{
    Debug("Process %d running as manager", processId);
    /* Allocate the Queue[] table for all customers. */
    Queue = (int*) calloc(nProcesses - nCompanies, sizeof(int));
    queueLen = 0;

    /* Allocate descriptors for nKillers. */
    Killers = (Killer*) calloc(nKillers, sizeof(Killer));

    /* Ensure that we have enough memory */
    if(Queue == NULL || Killers == NULL)
    {
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    /* Initialize all killers. */
    for (int ik = 0; ik < nKillers; ik++)
    {
        /* Customers not assigned yet */
        Killers[ik].client = NO_CLIENT;
    }

    /* Initialize the mutex for protection of the list with killers. */
    pthread_mutex_init(&killersMutex, NULL);
    /* Create the dedicated thread for the role agent of killers. */
    pthread_create(&agentThread, NULL, &RunAgent, NULL);

    /* Now, the company can focus on incomming messages. */
    while(1)
    {
        ReceiveMessages();
    }
}
