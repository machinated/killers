#ifndef _MESSAGING
#define _MESSAGING
//#define _BSD_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <mpi.h>
#include "message.h"
#include "timer.h"

/* Common functions used for communication between processes. */
void ReceiveAny(Message* msgP, int* tag, int* sender);

void Send(void* data, int dest, int tag);

void SendToAll(void* data, int tag);    // TODO

void SendAck(int pid, int ack);

void SendTake(int company, int killer);

void SendRelease(int company, int killer);

#endif
