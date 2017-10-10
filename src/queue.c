#include "messaging.h"
#include "queue.h"

void InitQueue(Queue* queue)
{
    assert(queue != NULL);

    queue->queueArray = (int*) malloc(nProcesses * sizeof(int));
    for (int qi = 0; qi < nProcesses; qi++)
    {
        queue[qi] = -1;
    }

    queue->length = 0;
}

int QueueContains(Queue* queue, int client)
{
    assert(queue != NULL);
    assert(client >= 0 && client < nProcesses);

    int* queueArray = queue->queueArray;
    for (int qi = 0; qi < queue->length; qi++)
    {
        assert(queueArray[qi] >= 0 && queueArray[qi] < nProcesses);
        if (queueArray[qi] == client)
        {
            return 1;
        }
    }
    return 0;
}

void QueueAdd(Queue* queue, int client)
{
    assert(!QueueContains(queue, client));

    int newIndex = queue->length;
    assert(newIndex >= 0 && newIndex < nProcesses);
    assert((queue->queueArray)[newIndex] == -1);
    (queue->queueArray)[newIndex] = client;
    queue->length += 1;
}

void QueueRemove(Queue* queue, int client)
{
    assert(queue != NULL);
    assert(client >= 0 && client < nProcesses);

    int* queueArray = queue->queueArray;
    int queueLen = queue->length;
    int qi;
    int found = 0;
    for (qi = 0; qi < queueLen; qi++)
    {
        if (queueArray[qi] == client)
        {
            found = 1;
            break;
        }
    }
    if (found)
    {
        for( ; qi < queueLen - 1; qi++)
        {
            queueArray[qi] = queueArray[qi+1];
            // SendUpdate(qi, client);
        }
        queue->length -= 1;
        // Debug("Removed %d from queue", client);
        // PrintQueue();
    }
    else
    {
        Error("Attempting to remove %d, but not in queue", client);
    }
}

int QueuePop(Queue* queue)
{
    assert(queue != NULL);
    
    int queueLen = queue->length;
    int* queueArray = queue->queueArray;
    if (queueLen < 1)
    {
        Debug("QueuePop: no elements");
        return NO_CLIENT;
    }
    int first = queueArray[0];
    for (int qi = 0; qi < queueLen - 1; qi++)
    {
        queueArray[qi] = queueArray[qi+1];
        // SendUpdate(qi, Queue[qi]);
    }
    queue->length -= 1;
    Debug("Popped %d from queue", first);
    // PrintQueue();
    return first;
}