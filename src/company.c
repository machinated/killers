#define _POSIX_C_SOURCE 199309L
#define _BSD_SOURCE
#include <stdlib.h>
#include <time.h>
#include <strings.h>
#include <assert.h>
#include <pthread.h>
#include "message.h"
#include "messaging.h"
#include "company.h"

#define KILLTIME 4000

int* queue;
int queueLen;
pthread_mutex_t queueMutex;

typedef struct timespec Timespec;

Timespec* killers;

void lockMutex(pthread_mutex_t* mutexP)
{
    int error = pthread_mutex_lock(mutexP);
    if (error)
    {
        Log("Error locking mutex: %d", error);
        exit(1);
    }
}

void unlockMutex(pthread_mutex_t* mutexP)
{
    int error = pthread_mutex_unlock(mutexP);
    if (error)
    {
        Log("Error unlocking mutex: %d", error);
        exit(1);
    }
}

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

void SendUpdate(int queuePos, int dest)
{
    MessageUpdate msgUpdate;
    msgUpdate.queueIndex = queuePos;
    Send(&msgUpdate, dest, TAG_UPDATE);
}

int AwaitAck(int client)
{
    Message msg;
    int msgSender;
    Receive(&msg, TAG_ACK, &msgSender);
    MessageAck* msgAckP = (MessageAck*) &(msg.data.data);
    int ack = msgAckP->ack;
    assert(ack == ACK_OK || ack == ACK_REJECT);
    if (msgSender == client)
    {
        return ack;
    }
    else
    {
        Log("ERROR: received ACK from %d when expected from %d",
            msgSender, client);
        exit(1);
    }
}

void QueueAdd(int client)
{
    //lockMutex(&queueMutex);
    queue[queueLen] = client;
    SendUpdate(queue[queueLen], client);
    queueLen++;
    //unlockMutex(&queueMutex);
}

void QueueRemove(int client)
{
    //lockMutex(&queueMutex);
    int found = 0;
    int qi;
    for (qi = 0; qi < queueLen; qi++)
    {
        if (queue[qi] == client)
        {
            found = 1;
            break;
        }
    }
    if (found)
    {
        for( ; qi < queueLen; qi++)
        {
            queue[qi] = queue[qi+1];
        }
        queueLen--;
    }
    else
    {
        Log("Attempting to remove %d, but not in queue", client);
    }
    //unlockMutex(&queueMutex);
}

void ReceiveRequest()
{
    Message msg;
    int client;

    Receive(&msg, TAG_REQUEST, &client);
    QueueAdd(client);
}

void ReceiveCancel()
{
    Message msg;
    int client;

    Receive(&msg, TAG_CANCEL, &client);
    QueueRemove(client);
}

void* ListenRequest(void* param __attribute__((unused)))
{
    while(1)
    {
        ReceiveRequest();
    }
}

void* ListenCancel(void* param __attribute__((unused)))
{
    while(1)
    {
        ReceiveCancel();
    }
}

#define TIMESPEC_ZERO(t) (t.tv_sec == 0 && t.tv_nsec == 0)

void SetKillerTimer(int killer)
{
    Timespec end;
    int32_t randNumber;
    random_r(randState, &randNumber);
    clock_gettime(CLOCK_MONOTONIC, &end);
    AddTime(&end, randNumber % (KILLTIME * 2));
    killers[killer] = end;
}

void NewJob(int ikiller)
{
    lockMutex(&queueMutex);
    if (!TIMESPEC_ZERO(killers[ikiller]))   // there was a previous job
    {
        assert(queueLen > 0);

        int oldClient = queue[0];
        SendUpdate(Q_DONE, oldClient);

        // shift queue left
        // remove finished job from queue
        queueLen--;
        for (int i=0; i < queueLen; i++)
        {
            queue[i] = queue[i+1];
            if (i != 0)
            {
                SendUpdate(i, queue[i]);
            }
        }
    }

    if (queueLen > 0)   // there is a new job
    {
        int qi;
        for (qi = 0; qi < queueLen; qi++)   // find client
        {
            int client = queue[qi];
            SendUpdate(Q_INPROGRESS, client);
            if (AwaitAck(client) == ACK_OK)
            {
                break;
            }
            else
            {
                QueueRemove(client);
            }
        }
        if (qi < queueLen)  // found client
        {
            // set finish time for new fob
            SetKillerTimer(ikiller);
        }
        else
        {
            killers[ikiller].tv_sec = 0;
            killers[ikiller].tv_nsec = 0;
        }
    }
    else                // no new job
    {
        killers[ikiller].tv_sec = 0;
        killers[ikiller].tv_nsec = 0;
    }
    unlockMutex(&queueMutex);
}

void RunCompany()
{
    queue = (int*) calloc(nProcesses, sizeof(int));
    queueLen = 0;
    pthread_mutex_init(&queueMutex, NULL);
    killers = (Timespec*) calloc(nKillers, sizeof(Timespec));
    int* inqueue = (int*) calloc(nProcesses, sizeof(int));

    pthread_t requestListener, cancelListener;
    pthread_create(&requestListener, NULL, &ListenRequest, NULL);
    pthread_create(&cancelListener, NULL, &ListenCancel, NULL);

    while(1)
    {
        bzero(inqueue, nProcesses);
        lockMutex(&queueMutex);
        for (int qi = 0; qi < queueLen; qi++)
        {
            inqueue[queue[qi]] = 1;
        }
        for (int client = nCompanies; client < nProcesses; client++)
        {
            if (inqueue[client] == 0)
            {
                SendUpdate(Q_AVAILABLE, client);
            }
        }
        unlockMutex(&queueMutex);

        for (int i = 0; i < nKillers; i++)
        {
            if (momentPassed(killers[i]))
            {
                NewJob(i);
            }
        }
        milisleep(1);
    }
}
