#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "common.h"
#include "logging.h"

/* This routine is a helper function for the Log() routine. */
void _Log(const char* format, va_list args)
{
    const char* preFormat = "PROCESS %*d CLK %*d:";
    char* string = (char*) calloc(150, sizeof(char));
    sprintf(string, preFormat, 3, processId, 6, localClock);

    strncat(string, format, 100);   // append format to string
    const char* nl = "\n";
    strcat(string, nl);

    vprintf(string, args);

    free(string);
}

/* This routine allows to log the specified information.
 * Note that this functionality needs to be enabled by the used <logPriority> level.
 */
void Log(int priority, const char* format, ...)
{
    if (priority >= logPriority)
    {
        va_list argList;
        va_start(argList, format);
        _Log(format, argList);
        va_end(argList);
    }
}

/* This routine can be used to print some debug information.
 * Note that this functionality needs to be enabled by the used <logPriority> level.
 */
void Debug(const char* format, ...)
{
    if (LOG_DEBUG >= logPriority)
    {
        va_list argList;
        va_start(argList, format);
        _Log(format, argList);
        va_end(argList);
    }
}

/* This routine can be used to print some additional information.
 * Note that this functionality needs to be enabled by the used <logPriority> level.
 */
void Info(const char* format, ...)
{
    if (LOG_INFO >= logPriority)
    {
        va_list argList;
        va_start(argList, format);
        _Log(format, argList);
        va_end(argList);
    }
}

/* This routine can be used to print information about the detected issue.
 * Note that this functionality needs to be enabled by the used <logPriority> level.
 */
void Error(const char* format, ...)
{
    if (LOG_ERROR >= logPriority)
    {
        va_list argList;
        va_start(argList, format);
        _Log(format, argList);
        va_end(argList);
    }
}
