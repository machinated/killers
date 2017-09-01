#define _POSIX_C_SOURCE 199309L
#define _BSD_SOURCE
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <pthread.h>
#include "message.h"
#include "messaging.h"
#include "company.h"

int* Queue;
int queueLen;

int* Jobs;

int agent;

void SendUpdate(int queuePos, int dest)
{
    MessageUpdate msgUpdate;
    msgUpdate.queueIndex = queuePos;
    Log("Sending UPDATE to %d, place in queue: %d", dest, queuePos);
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
    Log("QUEUE: %s", str);
    free(str);
}

void QueueAdd(int client)
{
    //lockMutex(&queueMutex);
    Queue[queueLen] = client;
    queueLen++;
    SendUpdate(queueLen - 1, client);
    assert(queueLen <= nProcesses);
    Log("Added %d to queue", client);
    PrintQueue();
    //unlockMutex(&queueMutex);
}

void QueueRemove(int client)
{
    //lockMutex(&queueMutex);
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
        Log("Removed %d from queue", client);
        PrintQueue();
    }
    else
    {
        Log("Attempting to remove %d, but not in queue", client);
    }
    //unlockMutex(&queueMutex);
}

int QueuePop()
{
    if (queueLen < 1)
    {
        Log("QueuePop: no elements");
        return -1;
    }
    int first = Queue[0];
    for (int qi = 0; qi < queueLen - 1; qi++)
    {
        Queue[qi] = Queue[qi+1];
        SendUpdate(qi, Queue[qi]);
    }
    queueLen--;
    Log("Popped %d from queue", first);
    PrintQueue();
    return first;
}

void NewJob(int killer)
{
    int oldClient = Jobs[killer];
    if (oldClient != -1)   // there was a previous job
    {
        SendUpdate(Q_DONE, oldClient);
        Jobs[killer] = -1;
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
        Jobs[killer] = client;
        if (client == -1)
        {
            SendAck(agent, ACK_REJECT);
        }
        else
        {
            SendAck(agent, ACK_OK);
        }
    }
    else
    {
        SendAck(agent, ACK_REJECT);
    }
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
            Log("Received KILLER_READY from %d, killer: %d", sender, killer);
            NewJob(killer);
            break;
        }
        default:
        {
            Log("Received message with invalid tag: %d", tag);
        }
    }
}

void RunCompany()
{
    Log("Process %d running as manager", processId);
    Queue = (int*) calloc(nProcesses, sizeof(int));
    queueLen = 0;

    Jobs = (int*) calloc(nKillers, sizeof(int));
    for (int job = 0; job < nKillers; job++)
    {
        Jobs[job] = -1;
    }

    agent = nCompanies + processId;

    while(1)
    {
        ReceiveMessages();
    }
}
