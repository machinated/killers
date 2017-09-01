#define _BSD_SOURCE
#include <mpi.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include "message.h"
#include "messaging.h"
#include "client.h"

#define SLEEPTIME 3000  // ms

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
    Send(NULL, pid, TAG_REQUEST);
}

void SendCancel(int pid)
{
    Send(NULL, pid, TAG_CANCEL);
}

void SendAck(int pid, int ack)
{
    MessageAck msgAck;
    msgAck.ack = ack;
    Send(&msgAck, pid, TAG_ACK);
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
            if (queuePos == Q_INPROGRESS)
            {
                if (state == INPROGRESS)
                {
                    SendAck(sender, ACK_REJECT);
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
            Log("ERROR: received message with invalid tag: %d", tag);
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
    State state = WAITING;

    reputations = calloc(nCompanies, sizeof(float));
    queues = calloc(nCompanies, sizeof(int));

    if (reputations == NULL || queues == NULL)
    {
        exit(1);
    }

    while(1)
    {
        switch (state)
        {
            case WAITING:
            {
                int32_t randNumber;
                random_r(randState, &randNumber);
                if (randNumber < RAND_MAX * (1.0/SLEEPTIME))
                {
                    state = NOQUEUE;
                    localClock++;
                    Log("looking for killer");
                }
                milisleep(1);
                break;
            }
            case NOQUEUE:
            {
                while(1)
                {
                    int company = ReceiveUpdate(state);
                    if (queues[company] == Q_AVAILABLE)
                    {
                        SendRequest(company);
                    }
                    else if (queues[company] > 0)
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
                        // check current queues
                        int request = 1;
                        for (int icompany = 0; icompany < nCompanies; icompany++)
                        {
                            int iqi = queues[icompany];
                            float irep = reputations[icompany];
                            if (Compare(2, rep, iqi, irep) == 1)
                            {
                                request = 0;
                            }
                        }
                        if (request)
                        {
                            SendRequest(company);
                        }
                    }
                    else if (queueIndex == Q_INPROGRESS)
                    {
                        state = INPROGRESS;
                        Log("murder in progress");
                        for (int icompany = 0; icompany < nCompanies; icompany++)
                        {
                            if (icompany != company)
                            {
                                SendCancel(icompany);
                            }
                        }
                        break;
                    }
                    else if (queueIndex > 0)
                    {
                        for (int i = 0; i<nCompanies; i++)
                        {
                            int iqi = queues[i];
                            float irep = reputations[i];
                            if (iqi > 0)
                            {
                                int res = Compare(queueIndex, rep, iqi, irep);
                                if (res == -1)
                                {
                                    SendCancel(i);
                                }
                                else if (res == 1)
                                {
                                    SendCancel(company);
                                }
                            }
                        }
                    }
                    else
                    {
                        Log("ERROR: invalid queue index: %d", queueIndex);
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
                        Log("request fulfilled");

                        int32_t randNumber;
                        random_r(randState, &randNumber);
                        float review = (5.0 * randNumber) / RAND_MAX;
                        Log("sending review of company %d; score = %f",
                            company, review);
                        SendReview(company, review);
                        state = WAITING;
                        break;
                    }
                }
                break;
            }
        }
        localClock++;
    }
}
