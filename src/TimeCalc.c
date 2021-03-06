#include "TimeCalc.h"
#include <sys/time.h>
#include <time.h>

void startTimeCalc(timeCalc *const tptr)
{
    gettimeofday(tptr, NULL);
}

double getTimeCalc(timeCalc *const tptr)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return difftime(now.tv_sec, tptr->tv_sec) + ((double)now.tv_usec - (double)tptr->tv_usec) / 1000000.0;
}
