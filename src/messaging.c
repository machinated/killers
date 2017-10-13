#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <mpi.h>
#include "common.h"
#include "logging.h"
#include "messaging.h"

/* Tag names for MPI statuses in messages */
const char* tag_names[20] = {"ENQUEUE", "CANCEL", "REQUEST", "ACK", "TAKE",
                            "RELEASE", "REVIEW"};

/* This routine updates the <localClock> if the given value is greater than the current one. */
void UpdateClock(uint64_t msgClk)
{
    if (msgClk > localClock)
    {
        localClock = msgClk;
    }
}

/* This routine obtains the first message of ANY type. */
void ReceiveAny(Message* msgP, int* tag, int* sender)
{
    MPI_Status stat;
    MPI_Recv(msgP, sizeof(Message), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG,
            MPI_COMM_WORLD, &stat);
    Debug("Received from %d, tag %s, message clock = %d",
        stat.MPI_SOURCE, tag_names[stat.MPI_TAG], msgP->data.clk);
    UpdateClock(msgP->data.clk);
    localClock++;
    *tag = stat.MPI_TAG;
    *sender = stat.MPI_SOURCE;
}

/* This routine sends a message based on the specified information:
 * <data> - Additional data to append to the composed message
 * <dest> - Rank of destination (integer)
 * <tag>  - Message tag (integer)
 */
void Send(void* data, int dest, int tag)
{
    Message msg;

    msg.data.clk = localClock;
    if (data != NULL)
    {
        memcpy(&(msg.data.data), data, 24);
    }
    Debug("Sending to %d, tag %s", dest, tag_names[tag]);
    MPI_Send(&msg, sizeof(Message), MPI_BYTE, dest, tag, MPI_COMM_WORLD);
    localClock++;
}

void SendToAll(void* data, int tag)
{
    for (int i = 0; i < nProcesses; i++)
    {
        if (i != processId)
        {
            Send(data, i, tag);
        }
    }
}

/* This routine allows to send a message to the specified <pid> recipient
 * with the TAG_ACK tag and additional <ack> information  (if any).
 */
void SendAck(int pid, int ack)
{
    MessageAck msgAck;
    msgAck.ack = ack;
    Debug("Sending ACK to %d, status %d", pid, ack);
    Send(&msgAck, pid, TAG_ACK);
}

void SendTake(int company, int killer)
{
    MessageTake msgTake;
    msgTake.company = company;
    msgTake.killer = killer;
    SendToAll(&msgTake, TAG_TAKE);
}

void SendRelease(int company, int killer)
{
    MessageTake msgRelease;
    msgRelease.company = company;
    msgRelease.killer = killer;
    SendToAll(&msgRelease, TAG_RELEASE);
}
