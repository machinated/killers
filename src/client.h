#ifndef _CLIENT
#define _CLIENT

void RunClient();

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

// TODO
void AbortRequest();

void CheckConfirmed();

void CheckCancel(int company);

// void lockQueues();
// void unlockQueues();
// void lockKillers();
// void unlockKillers();

void InitKillers();
void InitQueues();
void Enqueue();
void CancelCompany(int company);
int GetFreeKiller(int company);
int GetFreeCompany(int* company, int* killer);
int TryTakeKiller();
void PrintReputations();
void HandleReview(MessageReview* msg);
void CheckConfirmed();
void CheckCancel(int company);
void AbortRequest();
void SendReview(int company, float review);

#endif
