#ifndef DEBUG_H
#define DEBUG_H

#if LOG_NDEBUG
#   ifndef __ANDROID__
#       include <stdio.h>
#           define ALOGD(__ARGS__...)    \
            {                           \
                fprintf(stdout, "%s:\t", LOG_TAG); \
                fprintf(stdout, __ARGS__);         \
                fprintf(stdout, "\n");             \
                fflush(stdout); \
            }
#       define ALOGE(__ARGS__...)             \
            {                                    \
                fprintf(stderr, "%s:\t", LOG_TAG); \
                fprintf(stderr, __ARGS__);         \
                fprintf(stderr, "\n");             \
                fflush(stdout); \
        }
#   endif /* __linux__ */
#else
#   define ALOGD(...)
#   define ALOGE(...)
#endif /* LOG_NDEBUG */

#endif // DEBUG_H
