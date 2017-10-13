#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "messaging.h"
#include "queue.h"
#include "common.h"
#include "logging.h"
#include "messaging.h"
#include "client.h"
#include "listener.h"

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

// Compare <company> with other companies and possibly leave queue
void CheckCancel(int company)
{
    Queue* queue = &(Queues[company]);
    int pos = QueueFind(queue, processId);
    if (pos == -1)
    {
        return;
    }
    float rep = reputations[company];

    for (int ic = 0; ic < nCompanies; ic++)
    {
        Queue* iqueue = &(Queues[ic]);
        int ipos = QueueFind(iqueue, processId);
        if (ipos == -1)
        {
            continue;
        }
        float irep = reputations[ic];

        int res = Compare(pos, rep, ipos, irep);
        if (res == -1)
        {
            Debug("Leaving queue for %d (rep %f) "
                  "in favor of %d (rep %f)",
                  ic, irep, company, rep);
            CancelCompany(ic);
        }
        else if (res == 1)
        {
            Debug("Leaving queue to %d (rep %f) "
                  "in favor of %d (rep %f)",
                  company, rep, ic, irep);
            CancelCompany(company);
        }
    }
}

// Check condition for REQUESTING->CONFIRMED state transition.
// If condition is true then change state to CONFIRMED
// and reset AckReceived array.
void CheckConfirmed()
{
    assert(state == REQUESTING);

    for (int client = 0; client < nProcesses; client++)
    {
        if (AckReceived[client] == 0)
        {
            return;
        }
    }

    for (int client = 0; client < nCompanies; client++)
    {
        AckReceived[client] = 0;
    }
    state = CONFIRMED;
}

void AbortRequest()
{
    assert(state == REQUESTING);

    for (int company = 0; company < nCompanies; company++)
    {
        AckReceived[company] = 0;
    }
    state = QUEUE;
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

// Listener thread function
void* Listen(__attribute__ ((unused)) void* arg)
{
    Message msg;
    int tag, sender;

    while(1)
    {
        ReceiveAny(&msg, &tag, &sender);
        pthread_mutex_lock(&threadMutex);

        if (sender == processId)
        {
            Error("Received message from self, tag: %d", tag);
            continue;
        }

        switch (tag)
        {
            case TAG_ENQUEUE:
            {
                for (int company = 0; company < nCompanies; company++)
                {
                    QueueAdd(&(Queues[company]), sender);
                }
                break;
            }

            case TAG_CANCEL:
            {
                MessageCancel* msgCancel = (MessageCancel*) &(msg.data.data);
                Queue* queue = &(Queues[msgCancel->company]);
                QueueRemove(queue, sender);
                CheckCancel(msgCancel->company);
                break;
            }

            case TAG_REQUEST:
            {
                MessageRequest* msgRequest = (MessageRequest*) &(msg.data.data);
                if (state == REQUESTING &&
                    msgRequest->company == requestedCompany &&
                    msgRequest->killer == requestedKiller)          // conflict
                {
                    if (sender < processId)
                    {
                        AbortRequest();
                        SendAck(sender, ACK_OK);
                    }
                    else
                    {
                        SendAck(sender, ACK_REJECT);
                    }
                }
                else if ((state == CONFIRMED || state == INPROGRESS) &&
                    (msgRequest->company == requestedCompany &&
                    msgRequest->killer == requestedKiller))
                {
                    SendAck(sender, ACK_REJECT);
                }
                // TODO note in Queues or somewhere ???
                break;
            }

            case TAG_ACK:
            {
                if (state == REQUESTING)
                {
                    MessageAck* msgAck = (MessageAck*) &(msg.data.data);
                    if (msgAck->ack == ACK_OK)
                    {
                        AckReceived[sender] = 1;
                    }
                    else if (msgAck->ack == ACK_REJECT)
                    {
                        assert(AckReceived[sender] == 0);
                        AbortRequest();
                    }
                    else
                    {
                        Error("Received invalid ACK value: %d from %d",
                            msgAck->ack, sender);
                    }
                }
                break;
            }

            case TAG_TAKE:
            {
                MessageTake* msgTake = (MessageTake*) &(msg.data.data);
                Killers[msgTake->company][msgTake->killer] = KILLER_BUSY;
                // Queue* queue = &(Queues[msgTake->company]);
                // QueueRemove(queue, sender);
                break;
            }

            case TAG_RELEASE:
            {
                MessageRelease* msgRelease = (MessageRelease*) &(msg.data.data);
                Killers[msgRelease->company][msgRelease->killer] = KILLER_FREE;
                break;
            }

            case TAG_REVIEW:
            {
                MessageReview* msgReview = (MessageReview*) &(msg.data.data);
                HandleReview(msgReview);
                break;
            }

            default:
            {
                Error("Received message of invalid type: %d", tag);
            }
        }
        pthread_mutex_unlock(&threadMutex);
        pthread_cond_signal(&messageReceivedCond);
    }
}
