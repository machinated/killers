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
    int status;
    Timespec timer;
} Killer;

// possible Killer.status values:
#define K_READY 0
#define K_BUSY 1
#define K_NOTIFICATION_SENT 2    // job done
                        // message KILLER_READY has been sent but not received yet

Killer* Killers;
pthread_mutex_t killersMutex;   // protects list "Killers"
pthread_cond_t wakeUpAgent;     // wake up agent when killer is sent by the thread running RunCompany

// queue states (queue index special values)
// queues[i] >= 0 <=> queues[i] is position in queue for company i
#define Q_CANCELLED -4
#define Q_NO_UPDATE_RECEIVED -3
#define Q_IN_PROGRESS -1
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
