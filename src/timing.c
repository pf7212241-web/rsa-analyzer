#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "timing.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

double monotonic_seconds(void) {
    double seconds = -1.0;
    /* Счётчик не зависит от перевода системных часов. */
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    if (QueryPerformanceFrequency(&frequency) &&
        QueryPerformanceCounter(&counter) && frequency.QuadPart > 0) {
        seconds = (double)counter.QuadPart / (double)frequency.QuadPart;
    }
#else
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) == 0) {
        seconds = (double)value.tv_sec + (double)value.tv_nsec / 1e9;
    }
#endif
    return seconds;
}
