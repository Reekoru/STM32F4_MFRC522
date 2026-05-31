#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>

#define DEBUG_LOG_ENABLED 1

#if DEBUG_LOG_ENABLED
    #define DEBUG_LOG(...) printf(__VA_ARGS__)
    #define DEBUG_LOG_HEX(label, data, length) do { \
        printf("%s: ", label); \
        for (size_t i = 0; i < length; i++) { \
            printf("%02X ", data[i]); \
        } \
        printf("\r\n"); \
    } while (0)
#else
    #define DEBUG_LOG(...) do {} while (0)
    #define DEBUG_LOG_HEX(label, data, length) do {} while (0)
#endif

#endif /* _UTILS_H */