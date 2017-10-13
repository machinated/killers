#ifndef _CLIENT
#define _CLIENT
#include <stdint.h>
#include <pthread.h>
#include "queue.h"

// Values of a customer's state
typedef enum State
{
    WAITING,       /* A customer has no new task/job yet for a killer. */
    NOQUEUE,       /* A customer has a task/job, but it is not in any queue yet.
                    * In this state the customer sends a request to all companies. */
    QUEUE,
    REQUESTING,
    CONFIRMED,
    INPROGRESS
} State;

/* Reputation of companies (one entry per each company). */
float* reputations;

uint8_t** Killers;      // Killers[n][k] -> state of killer k in company n

typedef enum KillerState
{
    KILLER_FREE,
    KILLER_BUSY
} KillerState;

Queue* Queues;          // Queue[n] is a queue for company n

uint8_t* AckReceived;
State state;

int requestedCompany;    // requested or used
int requestedKiller;

pthread_cond_t messageReceivedCond;

pthread_mutex_t threadMutex;

void CancelCompany(int company);

void RunClient();

// void InitKillers();
// void InitQueues();
// void Enqueue();
// void CancelCompany(int company);
// int GetFreeKiller(int company);
// int GetFreeCompany(int* company, int* killer);
// int TryTakeKiller();
// void PrintReputations();
// void HandleReview(MessageReview* msg);
// void CheckConfirmed();
// void CheckCancel(int company);  // should we cancel queue for this company
// void SendReview(int company, float review);

#endif
