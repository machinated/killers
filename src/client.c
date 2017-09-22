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

/* FIXME - remove it if rredundant */
//#define SLEEPTIME 3000  // ms


/* Values of a customer's state */
typedef enum State
{
    WAITING,       /* A customer has no new task/job yet for a killer. */
    NOQUEUE,       /* A customer has a task/job, but it is not in any queue yet.
                    * In this state the customer sends a request to all companies. */
    QUEUE,         /* A customer has been waiting for infomration, that the task/job is in progress by one of these companies. In this state the customer considers new review ratings (if any). */
    INPROGRESS     /* The task/job is in progress. Awaiting for conformation that it is completed. In this state the customer sends notification to other customers about his review rating. */
} State;

/* Reputation of companies (one entry per each company). */
float* reputations;

/* The queues[N] table contain exactly one entry per each company.
 * Each entry represent the current status of the assigned place in a company's queue.
 * A nonnegative value represent the assigned place in a queue.
 * For other negative values see Q_CANCELLED, Q_NO_UPDATE_RECEIVED,  */
int* queues;


/* Send a 'TAG_REQUEST' to the specified company if a place in its queue is unknown. */
void SendRequest(int pid)
{
    assert(queues[pid] == Q_NO_UPDATE_RECEIVED);
    Send(NULL, pid, TAG_REQUEST);
}


/* Send a 'CANCEL' to the specified company if a place in its queue has been assigned already (i.e. value is greater than or equel to zero).
 * Note that this place is marked as 'CANCELLED' by the customer immediatelly after this message is sent. */
void SendCancel(int pid)
{
    assert(queues[pid] >= 0);
    Send(NULL, pid, TAG_CANCEL);
    queues[pid] = Q_CANCELLED;
}


/* Handle the obtained review message 'msg' */
void HandleReview(MessageReview* msg)
{
    reputations[msg->company] = reputations[msg->company] * 0.9 +
                                msg->review * 0.1;
}


/* Handle all received messages of the following types:
 * TAG_REVIEW, TAG_UPDATE. Other will be ignored.
 * This function terminates when the first message of type TAG_UPDATE is obtained.
 */
int ReceiveUpdate(State state)
{
    Message msg;
    int tag, sender;

    while(1)
    {
        /* Obtain any message. */
        ReceiveAny(&msg, &tag, &sender);
        /* Handle review comments sent by other customers */
        if (tag == TAG_REVIEW)
        {
            MessageReview* msgReviewP = (MessageReview*) &(msg.data.data);
            HandleReview(msgReviewP);
        }
        /* Handle a message of type 'UPDATE' from a company */
        else if (tag == TAG_UPDATE)
        {
            MessageUpdate* msgUpdateP = (MessageUpdate*) &(msg.data.data);
            int queuePos = msgUpdateP->queueIndex;
            /* At this point we ignore any valid position in a queue, as the reservation has been already cancelled by the customer */
            if (queues[sender] == Q_CANCELLED)
            {
                Debug("Ignored UPDATE from %d, position in queue = %d",
                    sender, queuePos);
                continue;
            }
            Debug("Received UPDATE from %d, position in queue = %d",
                sender, queuePos);
            /* We have been waiting for the first 'UPDATE' with status 'IN_PROGRESS'.
             * For this first lucky company we will sent ACK_OK change the customer's status to 'INPROGRESS' and we will sent ACK_OK. If this is a */
            if (queuePos == Q_IN_PROGRESS)
            {
                /* The current customer's state is QUEUE */
                if (state == QUEUE)
                {
                    /* Accept this first lucky company. */
                    SendAck(sender, ACK_OK);
                }
                else /* Reject other propositions as the current task/job is already in progress. */
                {
                    SendAck(sender, ACK_REJECT);
                    queues[sender] = Q_CANCELLED;
                    return sender;
                }
            }
            /* Update information about the current position in a queue. */
            queues[sender] = queuePos;
            /* Return when the first message of the TAG_UPDATE type has been obtained. */
            return sender;
        }
        else
        {
            Debug("ERROR: received message with invalid tag: %d", tag);
        }
    }
}

/* Send notification to all customers about the current <review>
 * of the completed job by the specified <company>.
 */
void SendReview(int company, float review)
{
    MessageReview msgReview;
    msgReview.company = company;
    msgReview.review = review;
    SendToClients(&msgReview, TAG_REVIEW);
}

/* FIXME - nothig to do here or functionality is missing.
 * Note that it is important to not send a rejection when both values represent the same company. */
int Compare(int queuePos1, float rep1, int queuePos2, float rep2)
{
    // if (queuePos1 < queuePos2 * 0.5 && rep1 > rep2 + 1)
    // {
    //     return -1;
    // }
    // if (queuePos2 < queuePos1 * 0.5 && rep2 > rep1 + 1)
    // {
    //     return 1;
    // }
    return 0;
}


/* Customer - create one and process its state machine */
void RunClient()
{
    Debug("Process %d running as client", processId);
    State state = WAITING;   /* Init state of the state machine */

    reputations = (float*) calloc(nCompanies, sizeof(float));
    queues = (int*) calloc(nCompanies, sizeof(int));

    Timespec queueTimer;
    Timespec requestTimer;

    assert(reputations != NULL && queues != NULL);

    /* Initialize information about assignes places in companies' queues and initialize reputation of all companies. */
    for (int company = 0; company < nCompanies; company++)
    {
        queues[company] = Q_NO_UPDATE_RECEIVED;
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
                for (int company = 0; company < nCompanies; company++)
                {
                    #ifdef DEBUG
                    if (queues[company] != Q_NO_UPDATE_RECEIVED)
                    {
                        Error("ERROR: unexpected place in queue %d for company %d",
                              queues[company], company);
                    }
                    #endif
                    SendRequest(company);
                }
                state = QUEUE;

            }
            /* Awaiting for information about the assigned places in the companies' queues. */
            case QUEUE:
            {
                while(1)
                {
                    /* Wait for the first message of type TAG_UPDATE. */
                    int company = ReceiveUpdate(state);
                    /* Obtain infomration about the assigned plase in a queue (if any). */
                    int queueIndex = queues[company];

                    /* Obtain the current infomration about reputation of the considered companies. */
                    float rep = reputations[company];

                    /* If we have the lucky company (i.e. the first one, which sent UPDATE with status IN_PROGRESS) */
                    if (queueIndex == Q_IN_PROGRESS)
                    {
                        state = INPROGRESS;
                        Info("Request accepted by company %d after %d ms",
                             company, elapsedMillis(&queueTimer));
                        startTimer(&requestTimer);
                        /* Notify other companies about resignation. */
                        for (int icompany = 0; icompany < nCompanies; icompany++)
                        {
                            if (icompany != company && queues[icompany] >= 0)
                            {
                                SendCancel(icompany);
                            }
                        }
                        break;
                    }
                    /* If we received information about the current place is a queue from some company then check whether this is a better proposition.
                    * FIXME it seems that nothig done here. Remove this as redundant or fix it */
                    else if (queueIndex >= 0)
                    {
                        for (int i = 0; i<nCompanies; i++)
                        {
                            int iqi = queues[i];
                            if (iqi == Q_CANCELLED)
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
                    else /* Detect issues */
                    {
                      if (queueIndex == Q_NO_UPDATE_RECEIVED)
                      {
                          Error("Unexpected queue status Q_NO_UPDATE_RECEIVED from company %d",
                                company);
                      }
                      else
                      {
                        Debug("ERROR: invalid queue index: %d", queueIndex);
                      }
                    }
                }
                break;
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
