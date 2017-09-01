#ifndef _MESSAGING
#define _MESSAGING
#define _BSD_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include "message.h"

void milisleep(long ms);

void Log(const char* format, ...);

uint64_t localClock;

int processId;

int nProcesses;

int nCompanies;

int nKillers;

struct random_data* randState;

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
