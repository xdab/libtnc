#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum
{
    LOG_LEVEL_STANDARD = 0,
    LOG_LEVEL_VERBOSE = 10,
    LOG_LEVEL_DEBUG = 20
} log_level_e;

static int _func_pad = -26;
static log_level_e _log_level = LOG_LEVEL_STANDARD;

// General informational messages that should always be visible (with function prefix).
#define LOG(str, ...)                                                                   \
    ({                                                                                  \
        fprintf(stderr, "i | %*s | " str "\n", _func_pad, __FUNCTION__, ##__VA_ARGS__); \
    })

// Detailed informational messages visible only with the verbose logging mode.
#define LOGV(str, ...)                                                                      \
    ({                                                                                      \
        if (_log_level >= LOG_LEVEL_VERBOSE)                                                \
            fprintf(stderr, "v | %*s | " str "\n", _func_pad, __FUNCTION__, ##__VA_ARGS__); \
    });

// Extremely detailed debugging messages visible only with the debug logging mode.
#define LOGD(str, ...)                                                                      \
    ({                                                                                      \
        if (_log_level >= LOG_LEVEL_DEBUG)                                                  \
            fprintf(stderr, "d | %*s | " str "\n", _func_pad, __FUNCTION__, ##__VA_ARGS__); \
    });

#define EXIT(str, ...)           \
    ({                           \
        LOG(str, ##__VA_ARGS__); \
        exit(EXIT_FAILURE);      \
    });

#define EXITIF(cond, exit_code, ...)      \
    ({                                    \
        if (cond)                         \
        {                                 \
            fprintf(stderr, __VA_ARGS__); \
            exit(exit_code);              \
        }                                 \
    });

#define nonnull(value, value_name) \
    EXITIF((void *)(value) == NULL, -2, "\n%s must not be NULL (%s:%d)\n\n", value_name, __FILE__, __LINE__);

#define nonzero(value, value_name) \
    EXITIF((value) == 0, -3, "\n%s must not be zero (%s:%d)\n\n", value_name, __FILE__, __LINE__);

#define nonnegative(value, value_name) \
    EXITIF((value) < 0, -3, "\n%s must be >= 0 (%s:%d)\n\n", value_name, __FILE__, __LINE__);

#define _assert(condition, message) \
    EXITIF((condition) == 0, -4, "\n%s (%s:%d)\n\n", message, __FILE__, __LINE__);

// Conditional assignment: if value equals a, replace with b
#define REPLACE_IF_a_WITH_b(value, a, b) \
    ({                                   \
        if ((value) == (a))              \
            (value) = (b);               \
    });

// Utility macros
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#endif
