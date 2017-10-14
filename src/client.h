#ifndef _CLIENT
#define _CLIENT
#include <stdint.h>
#include <pthread.h>
#include "queue.h"

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

int requestedCompany;    // requested or used
int requestedKiller;

pthread_cond_t messageReceivedCond;

pthread_mutex_t threadMutex;

void CancelCompany(int company);

void HandleReview(MessageReview* msg);

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
