#ifndef _MESSAGING
#define _MESSAGING
#define _BSD_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "message.h"

typedef struct timespec Timespec;

void milisleep(long ms);

void Log(int priority, const char* format, ...);

void Debug(const char* format, ...);

void Info(const char* format, ...);

void Error(const char* format, ...);

int logPriority;

#define LOG_TRACE 0
#define LOG_DEBUG 1
#define LOG_INFO 2
#define LOG_WARNING 3
#define LOG_ERROR 4

uint64_t localClock;

int processId;

int nProcesses;

int nCompanies;

int nKillers;

int SLEEPTIME;

int KILLTIME;

typedef struct Killer {
    int client;
    Timespec timer;
} Killer;

Killer* Killers;
pthread_mutex_t killersMutex;

//#define Q_UNKNOWN 0
#define Q_AVAILABLE -3
#define Q_INPROGRESS -1
#define Q_DONE -2

void Receive(Message* msgP, int tag, int* sender);

void ReceiveAll(Message* msgP, int* tag, int* sender);

void Send(void* data, int dest, int tag);

void SendToAll(void* data, int tag);

void SendToCompanies(void* data, int tag);

void SendToClients(void* data, int tag);

void SendAck(int pid, int ack);

int AwaitAck(int pid);

#endif
