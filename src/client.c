// #define _BSD_SOURCE
#define _POSIX_C_SOURCE 199309L
#include <mpi.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "message.h"
#include "messaging.h"
#include "timer.h"
#include "utils.h"
#include "queue.h"
#include "logging.h"
#include "common.h"
#include "listener.h"
#include "client.h"

pthread_t listenerThread;

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

// Add self to all queues and send TAG_ENQUEUE to every other process
void Enqueue()
{
    for (int company = 0; company < nCompanies; company++)
    {
        QueueAdd(&(Queues[company]), processId);
    }
    SendToAll(NULL, TAG_ENQUEUE);
}

// Remove self from local queue associated with company
// and send TAG_CANCEL to every other process
void CancelCompany(int company)
{
    // Debug("CancelCompany, company = %d", company);
    Queue* queue = &(Queues[company]);
    QueueRemove(queue, processId);
    MessageCancel msgCancel;
    msgCancel.company = company;
    SendToAll(&msgCancel, TAG_CANCEL);
}

// helper function for GetFreeCompany
int _GetFreeKiller(int company)
{
    uint8_t* companyKillers = Killers[company];

    for (int killer = 0; killer < nKillers; killer++)
    {
        if (companyKillers[killer] == KILLER_FREE)
        {
            return killer;
        }
    }

    return -1;
}

// Search local queues for available killer.
// If found set values of *company and *killer and return 0.
// If NOT found return -1.
int GetFreeCompany(int* company, int* killer)
{
    for (int ic = 0; ic < nCompanies; ic++)
    {
        if (Queues[ic].queueArray[0] == processId)
        {
            int ikiller = _GetFreeKiller(ic);
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
    if (GetFreeCompany(&requestedCompany, &requestedKiller))
    {
        return 0;
    }

    MessageRequest msg;
    msg.company = requestedCompany;
    msg.killer = requestedKiller;
    SendToAll(&msg, TAG_REQUEST);
    return 1;
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

void HandleReview(MessageReview* msg)
{
    reputations[msg->company] = reputations[msg->company] * 0.9 +
                                msg->review * 0.1;
    PrintReputations();
}

// Send notification to all customers about the current <review>
// of the completed job by the specified <company>.
void SendReview(int company, float review)
{
    MessageReview msgReview;
    msgReview.company = company;
    msgReview.review = review;
    HandleReview(&msgReview);   // update local reputation
    SendToAll(&msgReview, TAG_REVIEW);
}

void RunClient()
{
    pthread_mutex_init(&threadMutex, NULL);

    Debug("Process %d started", processId);
    state = WAITING;   /* Init state of the state machine */

    reputations = (float*) calloc(nCompanies, sizeof(float));
    assert(reputations != NULL);

    for (int company = 0; company < nCompanies; company++)
    {
        reputations[company] = 3.0;
    }

    InitKillers();
    InitQueues();

    Timespec queueTimer;
    Timespec requestTimer;

    AckReceived = (uint8_t*) calloc(nProcesses, sizeof(uint8_t));
    assert(AckReceived != NULL);

    requestedCompany = -1;
    requestedKiller = -1;

    pthread_create(&listenerThread, NULL, &Listen, NULL);

    /* Process the customer's state machine */
    while(1)
    {
        pthread_mutex_lock(&threadMutex);
        switch (state)
        {
            /* No new task from this customer yet */
            case WAITING:
            {
                if (rand() < RAND_MAX * (1.0/SLEEPTIME))
                {
                    state = NOQUEUE;
                    // localClock++;
                    startTimer(&queueTimer);
                    Info("Decision made by the customer: looking for a killer");
                }
                break;
            }
            /* The customer has a task for a killer, so find one. No place in a queue yet. */
            case NOQUEUE:
            {
                /* Send a request to all companies. */
                Enqueue();
                state = QUEUE;
                break;
            }
            /* Awaiting for information about the assigned places in the companies' queues. */
            case QUEUE:
            {
                int success = TryTakeKiller();  // sends TAG_REQUEST on success
                if (success)
                {
                    Info("Now requesting company %d, killer %d",
                        requestedCompany, requestedKiller);
                    state = REQUESTING;
                }
                else
                {
                    pthread_cond_wait(&messageReceivedCond, &threadMutex);
                }
                break;
            }
            case REQUESTING:
            {
                pthread_cond_wait(&messageReceivedCond, &threadMutex);
                break;
            }
            case CONFIRMED:
            {
                for (int company = 0; company < nCompanies; company++)
                {
                    if (QueueContains(&(Queues[company]), processId))
                    {
                        CancelCompany(company);
                    }
                }
                Killers[requestedCompany][requestedKiller] = KILLER_BUSY;
                SendTake(requestedCompany, requestedKiller);
                state = INPROGRESS;
                startTimer(&requestTimer);
                Info("Started using company %d, killer %d",
                     requestedCompany, requestedKiller);
                break;
            }
            /* The task/job has been started. Awaiting for completion. */
            case INPROGRESS:
            {
                if (rand() < RAND_MAX * (1.0/KILLTIME)) // job finished
                {
                    Info("Request fulfilled in %d ms",
                         elapsedMillis(&requestTimer));

                    Killers[requestedCompany][requestedKiller] = KILLER_FREE;
                    SendRelease(requestedCompany, requestedKiller);

                    /* Determine a review rating for the company. */
                    float review = (5.0 * rand()) / RAND_MAX;
                    Debug("sending review of company %d; score = %f",
                        requestedCompany, review);
                    /* Send notification with review rating to all customers. */
                    SendReview(requestedCompany, review);

                    /* Now the job/task is done. The customer can consider a next one (if any). */
                    state = WAITING;
                    requestedCompany = -1;
                    requestedKiller = -1;
                    break;
                }
                break;
            }
        }
        pthread_mutex_unlock(&threadMutex);
        milisleep(1);
        localClock++;
    }
}
