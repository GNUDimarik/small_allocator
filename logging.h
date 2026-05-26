#ifndef LOGGING_H
#define LOGGING_H

#if !LOG_NDEBUG
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
                fprintf(stderr, "ERROR: %s:\t", LOG_TAG); \
                fprintf(stderr, __ARGS__);         \
                fprintf(stderr, "\n");             \
                fflush(stderr); \
        }
#   endif /* __linux__ */
#else
#   define ALOGD(...)
#   define ALOGE(...)
#endif /* LOG_NDEBUG */

#endif //LOGGING_H
