#ifndef _LOGGING
#define _LOGGING

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

#endif
