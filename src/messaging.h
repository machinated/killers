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

int processId;     /* ID of the current process. */

int nProcesses;    /* The total number of processes */

int nCompanies;    /* The number of processes assigned to companies */

int nKillers;      /* The number of killers per a company. */

int SLEEPTIME;     /* How long a customer thinks about a new task/job (max value). */

int KILLTIME;      /* How much time a killer need to complete his task/job (max period of time). */


/* Descriptor of a killer */
typedef struct Killer {
    int client;                /* The current customer, which is served */
    int status;                /* Status of the killer */
    Timespec timer;            /* Time related with the current task ?? (time to the end)?? */
} Killer;


/* Possible values of a Killer's status 'Killer.status': */
#define K_READY 0              /* Awaiting for a job */
#define K_BUSY 1               /* Killer is busy - job in progress. */
#define K_NOTIFICATION_SENT 2  /* Job done by a killer. Message 'KILLER_READY' has been sent but it is not processed yet. */


Killer* Killers;                /* The list of killers of the current company. */
pthread_mutex_t killersMutex;   /* Mutex to protect the list of 'Killers' */
pthread_cond_t wakeUpAgent;     /* Wake up the killers' agent when a killer is assigned by the Manager (i.e the thread running the RunCompany() routine). */


/* States related with a queue, which can be sent via a message between a customer and a company.
 * These values are also used in the customer's queue[] table.
 *  Note that a nonnegative value obtained from a company represents the assigned position in a queue.
 *  Other special values are listed below:
 */
#define Q_CANCELLED -4               /* A reservarion has been cancelled by a customer */
#define Q_NO_UPDATE_RECEIVED -3      /* Unexpected (init) state of a queue. */
#define Q_IN_PROGRESS -1             /* Notification from a company that a task is in progress now. */
#define Q_DONE -2                    /* Notification from a company that the task has been completed. */



/* Common functions used for communication between processes. */


void Receive(Message* msgP, int tag, int* sender);

void ReceiveAny(Message* msgP, int* tag, int* sender);

void Send(void* data, int dest, int tag);

/* TBD
 * Not used - remove as redundant
 */
void SendToAll(void* data, int tag);

void SendToCompanies(void* data, int tag);

void SendToClients(void* data, int tag);

void SendAck(int pid, int ack);

int AwaitAck(int pid);


#endif
