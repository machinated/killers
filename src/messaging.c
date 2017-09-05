#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <mpi.h>
#include "messaging.h"

const char* tag_names[20] = {"0", "ACK", "2", "REQUEST", "UPDATE", "CANCEL",
                            "REVIEW", "KILLER_READY"};

void milisleep(long ms)
{
    struct timespec sleepTime;
    sleepTime.tv_sec =  (time_t)(ms / 1000);
    sleepTime.tv_nsec = (long)((ms - (sleepTime.tv_sec * 1000)) * 1000);
    if (nanosleep(&sleepTime, NULL) != 0)
    {
        printf("Nanosleep error: %d\n", errno);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

void _Log(const char* format, va_list args)
{
    const char* preFormat = "PROCESS %*d CLK %*d:";
    char* string = (char*) calloc(150, sizeof(char));
    sprintf(string, preFormat, 3, processId, 6, localClock);

    strncat(string, format, 100);   // append format to string
    const char* nl = "\n";
    strcat(string, nl);

    vprintf(string, args);

    free(string);
}

void Log(int priority, const char* format, ...)
{
    if (priority >= logPriority)
    {
        va_list argList;
        va_start(argList, format);
        _Log(format, argList);
        va_end(argList);
    }
}

void Debug(const char* format, ...)
{
    if (LOG_DEBUG >= logPriority)
    {
        va_list argList;
        va_start(argList, format);
        _Log(format, argList);
        va_end(argList);
    }
}

void Info(const char* format, ...)
{
    if (LOG_INFO >= logPriority)
    {
        va_list argList;
        va_start(argList, format);
        _Log(format, argList);
        va_end(argList);
    }
}

void Error(const char* format, ...)
{
    if (LOG_ERROR >= logPriority)
    {
        va_list argList;
        va_start(argList, format);
        _Log(format, argList);
        va_end(argList);
    }
}

void UpdateClock(uint64_t msgClk)
{
    if (msgClk > localClock)
    {
        localClock = msgClk;
    }
}

void Receive(Message* msgP, int tag, int* sender)
{
    MPI_Status stat;
    MPI_Recv(msgP, sizeof(Message), MPI_BYTE, MPI_ANY_SOURCE, tag,
            MPI_COMM_WORLD, &stat);
    assert(tag == stat.MPI_TAG);
    Debug("Received from %d, tag %s, message clock = %d",
        stat.MPI_SOURCE, tag_names[tag], msgP->data.clk);
    UpdateClock(msgP->data.clk);
    localClock++;
    *sender = stat.MPI_SOURCE;
}

void ReceiveAll(Message* msgP, int* tag, int* sender)
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

void Send(void* data, int dest, int tag)
{
    Message msg;
    // Message* message = malloc(sizeof(Message));
    // if (message == NULL)
    // {
    //     Error("malloc error");
    // }
    //message->pid = processId;
    msg.data.clk = localClock;
    if (data != NULL)
    {
        memcpy(&(msg.data.data), data, 24);
    }
    Debug("Sending to %d, tag %s", dest, tag_names[tag]);
    MPI_Send(&msg, sizeof(Message), MPI_BYTE, dest, tag, MPI_COMM_WORLD);
    // free(message);
    localClock++;
}

void SendToAll(void* data, int tag)
{
    for(int i = 0; i < nProcesses; i++)
    {
        Send(data, i, tag);
    }
}

void SendToCompanies(void* data, int tag)
{
    for (int i = 0; i < nCompanies; i++)
    {
        Send(data, i, tag);
    }
}

void SendToClients(void* data, int tag)
{
    for (int i = nCompanies; i < nProcesses; i++)
    {
        Send(data, i, tag);
    }
}

void SendAck(int pid, int ack)
{
    MessageAck msgAck;
    msgAck.ack = ack;
    Debug("Sending ACK to %d, status %d", pid, ack);
    Send(&msgAck, pid, TAG_ACK);
}

int AwaitAck(int pid)
{
    Message msg;
    int msgSender;
    Receive(&msg, TAG_ACK, &msgSender);
    MessageAck* msgAckP = (MessageAck*) &(msg.data.data);
    int ack = msgAckP->ack;
    assert(ack == ACK_OK || ack == ACK_REJECT);
    if (msgSender == pid)
    {
        Debug("Received ACK from %d, status %d", pid, ack);
        return ack;
    }
    else
    {
        Error("ERROR: received ACK from %d when expected from %d",
            msgSender, pid);
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
}
