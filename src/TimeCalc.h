#pragma once
typedef struct timeval wallclock_t;

void startTimeCalc(wallclock_t *const tptr);
double getTimeCalc(wallclock_t *const tptr);
