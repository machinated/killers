#define _BSD_SOURCE
#include <mpi.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include "message.h"
#include "messaging.h"
#include "timer.h"
#include "queue.h"
#include "client.h"

void InitKillers()
{
    Killers = (uint8_t**) malloc(nCompanies * sizeof(uint8_t*));
    assert(Killers != NULL);

    for (int company = 0; company < nCompanies; company++)
    {
        Killers[company] = (uint8_t*) malloc(nKillers * sizeof(uint8_t));
        for (int killer = 0; killer < nKillers; killer++)
        {
            Killers[company][killer] = KILLER_FREE;
        }
    }
}

void InitQueues()
{
    Queues = (Queue*) malloc(nCompanies * sizeof(Queue));
    assert(Queues != NULL);

    for (int company = 0; company < nCompanies; company++)
    {
        InitQueue(&(Queues[company]));
    }
}

void Enqueue()
{
    SendToAll(NULL, TAG_ENQUEUE);

    for (int company = 0; company < nCompanies; company++)
    {
        QueueAdd(&(Queues[company]), processId);
    }
    SendToAll(NULL, TAG_ENQUEUE);
}

void CancelCompany(int company)
{
    Queue* queue = Queues[company];
    QueueRemove(queue, processId);
    MessageCancel msgCancel;
    msgCancel.company = prodcessId;
    SendToAll(&msgCancel, TAG_CANCEL);
}

int GetFreeKiller(int company)
{
    uint8_t companyKillers = Killers[company];

    for (int killer = 0; killer < nKillers; killer++)
    {
        if (companyKillers[killer] = KILLER_FREE)
        {
            return killer;
        }
    }

    return -1;
}

int GetFreeCompany(int* company, int* killer)
{
    for (int ic = 0; ic < nCompanies; ic++)
    {
        if (Queues[ic][0] == processId)
        {
            int ikiller = GetFreeKiller(ic);
            if (ikiller != -1)
            {
                *company = ic;
                *killer = ikiller;
                return 0;
            }
        }
    }
    return -1;
}

int TryTakeKiller()
{
    int company, killer;
    if (GetFreeCompany(&company, &killer))
    {
        return 0;
    }

    MessageRequest msg;
    msg.company = company;
    msg.killer = killer;
    SendToAll(&msg, TAG_REQUEST);
    return 1;

    // uint8_t* ackReceived = calloc(nProcesses);
}

void PrintReputations()
{
    if (logPriority <= LOG_DEBUG)
    {
        char* str = (char*) calloc(nCompanies * 5, sizeof(char));
        char elem[5];
        for (int ci = 0; ci < nCompanies; ci++)
        {
            sprintf(elem, "%.2f ", reputations[ci]);
            strncat(str, elem, 5);
        }
        Debug("REPUTATIONS: %s", str);
        free(str);
    }
}

/* Handle the obtained review message 'msg' */
void HandleReview(MessageReview* msg)
{
    reputations[msg->company] = reputations[msg->company] * 0.9 +
                                msg->review * 0.1;
    PrintReputations();
}

void CheckConfirmed()
{
    assert(state == REQUESTING);

    for (int company = 0; company < nCompanies; company++)
    {
        if (AckReceived[company] == 0)
        {
            return;
        }
    }

    for (int company = 0; company < nCompanies; company++)
    {
        AckReceived[company] = 0;
    }
    state = CONFIRMED;
}

void CheckCancel(int company)
{
    Queue* queue = Queues[company];
    int pos = QueueFind(queue, processId);
    float rep = reputations[company];

    for (int ic = 0; ic < nCompanies; ic++)
    {
        Queue* iqueue = Queues[ic];
        int ipos = QueueFine(iqueue, processId);
        float irep = reputations[ic];

        int res = Compare(pos, rep, ipos, irep);
        if (res == -1)
        {
            Debug("Leaving queue for %d (rep %f) "
                  "in favor of %d (rep %f)",
                  i, irep, company, rep);
            CancelCompany(ic);
        }
        else if (res == 1)
        {
            Debug("Leaving queue to %d (rep %f) "
                  "in favor of %d (rep %f)",
                  company, rep, i, irep);
            SendCancel(company);
        }
    }
}

void AbortRequest()
{
    for (int company = 0; company < nCompanies; company++)
    {
        AckReceived[company] = 0;
    }
    state = QUEUE;
}


/* Send notification to all customers about the current <review>
 * of the completed job by the specified <company>.
 */
void SendReview(int company, float review)
{
    MessageReview msgReview;
    msgReview.company = company;
    msgReview.review = review;
    SendToAll(&msgReview, TAG_REVIEW);
}

/* Note that it is important to not send a rejection when both values represent the same company. */
int Compare(int queuePos1, float rep1, int queuePos2, float rep2)
{
    // if (queuePos1 < queuePos2 * 0.5 && rep1 > rep2 + 1)
    if (queuePos1 < queuePos2 && rep1 > rep2)
    {
        return -1;
    }
    // if (queuePos2 < queuePos1 * 0.5 && rep2 > rep1 + 1)
    if (queuePos2 < queuePos1 && rep2 > rep1)
    {
        return 1;
    }
    return 0;
}

void RunClient()
{
    Debug("Process %d started", processId);
    state = WAITING;   /* Init state of the state machine */

    reputations = (float*) calloc(nCompanies, sizeof(float));
    InitKillers();
    InitQueues();

    Timespec queueTimer;
    Timespec requestTimer;

    assert(reputations != NULL && queues != NULL);

    for (int company = 0; company < nCompanies; company++)
    {
        reputations[company] = 3.0;
    }

    /* Process the customer's state machine */
    while(1)
    {
        switch (state)
        {
            /* No new task from this customer yet */
            case WAITING:
            {
                if (rand() < RAND_MAX * (1.0/SLEEPTIME))
                {
                    state = NOQUEUE;
                    localClock++;
                    startTimer(&queueTimer);
                    Info("Decision made by the customer: looking for a killer");
                }
                milisleep(1);
                break;
            }
            /* The customer has a task for a killer, so find one. No place in a queue yet. */
            case NOQUEUE:
            {
                /* Send a request to all companies. */
                Enqueue();
                state = QUEUE;

            }
            /* Awaiting for information about the assigned places in the companies' queues. */
            case QUEUE:
            {
                int success = TryTakeKiller();
                if (success)
                {
                    // Info(...);
                    state = REQUESTING;
                }
                else
                {
                    int tag = ReceiveMessage();
                    // handle messages
                }
                break;
            }
            case REQUESTING:
            {
                int tag = ReceiveMessage();
            }
            case CONFIRMED:
            {
                // XXX send cancel
            }

            /* The task/job has been started. Awaiting for completion. */
            case INPROGRESS:
            {
                while(1)
                {
                    /* Wait for any message of type 'TAG_UPDATE' */
                    int company = ReceiveUpdate(state);
                    /* If received confirmation that the job/task is done */
                    if (queues[company] == Q_DONE)
                    {
                        Info("Request fulfilled in %d ms",
                             elapsedMillis(&requestTimer));

                        /* Determine a review rating for the company. */
                        float review = (5.0 * rand()) / RAND_MAX;
                        Debug("sending review of company %d; score = %f",
                            company, review);
                        /* Send notification with review rating to all customers. */
                        SendReview(company, review);

                        /* Now the job/task is done. The customer can consider a next one (if any). */
                        state = WAITING;
                        /* Cleanup the information about queues before the next round. */
                        for (int company = 0; company < nCompanies; company++)
                        {
                            queues[company] = Q_NO_UPDATE_RECEIVED;
                        }
                        break;
                    }
                }
                break;
            }
        }
        localClock++;
    }
}
