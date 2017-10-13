#ifndef _TIMER
#define _TIMER
#include <time.h>

typedef struct timespec Timespec;

void startTimer(Timespec* timer);

unsigned long elapsedMillis(Timespec* timer);

#endif
