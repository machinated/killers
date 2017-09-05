#define _BSD_SOURCE
#include <mpi.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include "message.h"
#include "messaging.h"
#include "timer.h"
#include "client.h"

//#define SLEEPTIME 3000  // ms

typedef enum State
{
    WAITING,
    NOQUEUE,
    QUEUE,
    INPROGRESS
} State;

float* reputations;

int* queues;

void SendRequest(int pid)
{
    assert(queues[pid] == Q_AVAILABLE);
    Send(NULL, pid, TAG_REQUEST);
}

void SendCancel(int pid)
{
    assert(queues[pid] >= 0);
    Send(NULL, pid, TAG_CANCEL);
    queues[pid] = Q_REJECTED;
}

void HandleReview(MessageReview* msg)
{
    reputations[msg->company] = reputations[msg->company] * 0.9 +
                                msg->review * 0.1;
}

int ReceiveUpdate(State state)
{
    Message msg;
    int tag, sender;

    while(1)
    {
        ReceiveAll(&msg, &tag, &sender);
        if (tag == TAG_REVIEW)
        {
            MessageReview* msgReviewP = (MessageReview*) &(msg.data.data);
            HandleReview(msgReviewP);
        }
        else if (tag == TAG_UPDATE)
        {
            MessageUpdate* msgUpdateP = (MessageUpdate*) &(msg.data.data);
            int queuePos = msgUpdateP->queueIndex;
            if (queues[sender] == Q_REJECTED)
            {
                Debug("Ignored UPDATE from %d, position in queue = %d",
                    sender, queuePos);
                continue;
            }
            Debug("Received UPDATE from %d, position in queue = %d",
                sender, queuePos);
            if (queuePos == Q_INPROGRESS)
            {
                if (state == INPROGRESS)
                {
                    SendAck(sender, ACK_REJECT);
                    queues[sender] = Q_REJECTED;
                    return sender;
                }
                else
                {
                    SendAck(sender, ACK_OK);
                }
            }
            queues[sender] = queuePos;
            return sender;
        }
        else
        {
            Debug("ERROR: received message with invalid tag: %d", tag);
        }
    }
}

void SendReview(int company, float review)
{
    MessageReview msgReview;
    msgReview.company = company;
    msgReview.review = review;
    SendToClients(&msgReview, TAG_REVIEW);
}

int Compare(int queuePos1, float rep1, int queuePos2, float rep2)
{
    if (queuePos1 < queuePos2 * 0.5 && rep1 > rep2 + 1)
    {
        return -1;
    }
    if (queuePos2 < queuePos1 * 0.5 && rep2 > rep1 + 1)
    {
        return 1;
    }
    return 0;
}

void RunClient()
{
    Debug("Process %d running as client", processId);
    State state = WAITING;

    reputations = (float*) calloc(nCompanies, sizeof(float));
    queues = (int*) calloc(nCompanies, sizeof(int));

    Timespec queueTimer;
    Timespec requestTimer;

    assert(reputations != NULL && queues != NULL);

    for (int company = 0; company < nCompanies; company++)
    {
        queues[company] = Q_AVAILABLE;
        reputations[company] = 3.0;
    }

    while(1)
    {
        switch (state)
        {
            case WAITING:
            {
                if (rand() < RAND_MAX * (1.0/SLEEPTIME))
                {
                    state = NOQUEUE;
                    localClock++;
                    startTimer(&queueTimer);
                    Info("looking for killer");
                }
                milisleep(1);
                break;
            }
            case NOQUEUE:
            {
                for (int company = 0; company < nCompanies; company++)
                {
                    #ifdef DEBUG
                    if (queues[company] != Q_AVAILABLE)
                    {
                        Error("ERROR: unexpected place in queue %d for company %d",
                              queues[company], company);
                    }
                    #endif
                    SendRequest(company);
                }
                state = QUEUE;
                while(1)
                {
                    int company = ReceiveUpdate(state);
                    if (queues[company] >= 0)
                    {
                        state = QUEUE;
                        break;
                    }
                }
                break;
            }
            case QUEUE:
            {
                while(1)
                {
                    int company = ReceiveUpdate(state);
                    int queueIndex = queues[company];
                    float rep = reputations[company];
                    if (queueIndex == Q_AVAILABLE)
                    {
                        Error("Unexpected queue status Q_AVAILABLE from company %d",
                              company);
                        // check current queues
                        // int request = 1;
                        // for (int icompany = 0; icompany < nCompanies; icompany++)
                        // {
                        //     int iqi = queues[icompany];
                        //     float irep = reputations[icompany];
                        //     if (Compare(2, rep, iqi, irep) == 1)
                        //     {
                        //         request = 0;
                        //         break;
                        //     }
                        // }
                        // if (request)
                        // {
                        //     SendRequest(company);
                        // }
                    }
                    else if (queueIndex == Q_INPROGRESS)
                    {
                        state = INPROGRESS;
                        Info("Request accepted by company %d after %d ms",
                             company, elapsedMillis(&queueTimer));
                        startTimer(&requestTimer);
                        for (int icompany = 0; icompany < nCompanies; icompany++)
                        {
                            if (icompany != company && queues[icompany] >= 0)
                            {
                                SendCancel(icompany);
                            }
                        }
                        break;
                    }
                    else if (queueIndex >= 0)
                    {
                        for (int i = 0; i<nCompanies; i++)
                        {
                            int iqi = queues[i];
                            if (iqi == Q_REJECTED)
                            {
                                continue;
                            }
                            float irep = reputations[i];
                            if (iqi >= 0)
                            {
                                int res = Compare(queueIndex, rep, iqi, irep);
                                if (res == -1)
                                {
                                    Debug("Leaving queue for %d (rep %f) "
                                          "in favor of %d (rep %f)",
                                          i, irep, company, rep);
                                    SendCancel(i);
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
                    }
                    else
                    {
                        Debug("ERROR: invalid queue index: %d", queueIndex);
                    }
                }
                break;
            }
            case INPROGRESS:
            {
                while(1)
                {
                    int company = ReceiveUpdate(state);
                    if (queues[company] == Q_DONE)
                    {
                        Info("Request fulfilled in %d ms",
                             elapsedMillis(&requestTimer));

                        float review = (5.0 * rand()) / RAND_MAX;
                        Debug("sending review of company %d; score = %f",
                            company, review);
                        SendReview(company, review);
                        state = WAITING;
                        for (int company = 0; company < nCompanies; company++)
                        {
                            queues[company] = Q_AVAILABLE;
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
