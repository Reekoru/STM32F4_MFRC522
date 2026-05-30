#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>

#define DEBUG_LOG_ENABLED 1

#if DEBUG_LOG_ENABLED
    #define DEBUG_LOG(...) printf(__VA_ARGS__)
#else
    #define DEBUG_LOG(...) do {} while (0)
#endif

#endif /* _UTILS_H */