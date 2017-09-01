#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <mpi.h>
#include "messaging.h"

void milisleep(long ms)
{
    struct timespec sleepTime;
    sleepTime.tv_sec =  (time_t)(ms / 1000);
    sleepTime.tv_nsec = (long)((ms - (sleepTime.tv_sec * 1000)) * 1000);
    if (nanosleep(&sleepTime, NULL) != 0)
    {
        printf("Nanosleep error: %d\n", errno);
        exit(1);
    }
}

void Log(const char* format, ...)
{
    const char* preFormat = "PROCESS %*d CLK %*d:";
    char* string = calloc(150, 1);
    sprintf(string, preFormat, 3, processId, 6, localClock);

    strncat(string, format, 100);   // append format to string
    const char* nl = "\n";
    strcat(string, nl);

    va_list argList;
    va_start(argList, format);

    vprintf(string, argList);
    va_end(argList);

    free(string);
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
    UpdateClock(msgP->data.clk);
    localClock++;
    *sender = stat.MPI_SOURCE;
}

void ReceiveAll(Message* msgP, int* tag, int* sender)
{
    MPI_Status stat;
    MPI_Recv(msgP, sizeof(Message), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG,
            MPI_COMM_WORLD, &stat);
    Log("Received from %d, tag = %d, message clock = %d",
        stat.MPI_SOURCE, stat.MPI_TAG, msgP->data.clk);
    UpdateClock(msgP->data.clk);
    localClock++;
    *tag = stat.MPI_TAG;
    *sender = stat.MPI_SOURCE;
}

void Send(void* data, int dest, int tag)
{
    Message *message = malloc(sizeof(Message));
    //message->pid = processId;
    message->data.clk = localClock;
    if (data != NULL)
    {
        memcpy(&(message->data), data, 16);
    }
    Log("Sending to %d, tag = %d", dest, tag);
    MPI_Send(message, sizeof(Message), MPI_BYTE, dest, tag, MPI_COMM_WORLD);
    free(message);
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
