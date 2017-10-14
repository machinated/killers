#ifndef _QUEUE
#define _QUEUE

typedef struct Queue
{
    int* queueArray;
    int length;
} Queue;

// Initialize structure pointed to by <queue>
void InitQueue(Queue* queue);

// Check whether queue contains client
// Return:
//   1 when found
//   0 when NOT found
int QueueContains(Queue* queue, int client);

// Add the specified <client> to the queue.
void QueueAdd(Queue* queue, int client);

// Remove the specified <client> from the queue.
void QueueRemove(Queue* queue, int client);

// Remove first element from queue.
// Return that element or NO_CLIENT when queue is empty
// int QueuePop(Queue* queue);

// Return index of client in queue or -1 if not found
int QueueFind(Queue* queue, int client);

#endif
