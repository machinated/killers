#define _POSIX_C_SOURCE 199309L
#define _BSD_SOURCE
#include <stdlib.h>
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

int* Queue;     // client[queuePos]
int queueLen;

pthread_t agentThread;

void SendUpdate(int queuePos, int dest)
{
    MessageUpdate msgUpdate;
    msgUpdate.queueIndex = queuePos;
    Debug("Sending UPDATE to %d, place in queue: %d", dest, queuePos);
    Send(&msgUpdate, dest, TAG_UPDATE);
}

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

int QueuePop()
{
    if (queueLen < 1)
    {
        Debug("QueuePop: no elements");
        return -1;
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
        Error("Error getting current time");
    }

    int millis = rand() % (KILLTIME * 2);

    AddTime(&timer, millis);
    Debug("Timer for killer %d set for %d ms", killer, millis);
    Killers[killer].timer = timer;
    pthread_cond_signal(&jobAdded);
}

void NewJob(int killer)
{
    pthread_mutex_lock(&killersMutex);
    int oldClient = Killers[killer].client;
    if (oldClient != -1)   // there was a previous job
    {
        SendUpdate(Q_DONE, oldClient);
        Killers[killer].client = -1;
    }

    if (queueLen > 0)   // there is a new job
    {
        int client = QueuePop();
        while (client != -1)
        {
            SendUpdate(Q_INPROGRESS, client);
            if (AwaitAck(client) == ACK_OK)
            {
                break;
            }
            client = QueuePop();
        }
        Killers[killer].client = client;
        if (client != -1)
        {
            SetKillerTimer(killer);
        }
    }
    else
    {
        Killers[killer].client = -1;
    }
    pthread_mutex_unlock(&killersMutex);
}

int GetFreeKiller()
{
    pthread_mutex_lock(&killersMutex);
    int killer = -1;
    for (int ik = 0; ik < nKillers; ik++)
    {
        if (Killers[ik].client == -1)
        {
            killer = ik;
            break;
        }
    }
    pthread_mutex_unlock(&killersMutex);
    return killer;
}

void ReceiveMessages()
{
    Message msg;
    int tag, sender;
    ReceiveAll(&msg, &tag, &sender);

    switch(tag)
    {
        case TAG_REQUEST:
        {
            QueueAdd(sender);
            if (queueLen == 1)
            {
                int killer = GetFreeKiller();
                if (killer != -1)
                {
                    NewJob(killer);
                }
            }
            break;
        }
        case TAG_CANCEL:
        {
            QueueRemove(sender);
            break;
        }
        case TAG_KILLER_READY:
        {
            MessageKillerReady* msgK = (MessageKillerReady*) &(msg.data.data);
            int killer = msgK->killer;
            Debug("Received KILLER_READY from %d, killer: %d", sender, killer);
            NewJob(killer);
            break;
        }
        default:
        {
            Debug("Received message with invalid tag: %d", tag);
        }
    }
}

void RunCompany()
{
    Debug("Process %d running as manager", processId);
    Queue = (int*) calloc(nProcesses, sizeof(int));
    queueLen = 0;

    Killers = (Killer*) calloc(nKillers, sizeof(Killer));

    if(Queue == NULL || Killers == NULL)
    {
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    for (int ik = 0; ik < nKillers; ik++)
    {
        Killers[ik].client = -1;
    }

    pthread_mutex_init(&killersMutex, NULL);
    pthread_create(&agentThread, NULL, &RunAgent, NULL);

    while(1)
    {
        ReceiveMessages();
    }
}
