
#ifndef __SYS_TIMESPEC_H__
#define __SYS_TIMESPEC_H__

#include <sys/types.h>

struct timespec {
    time_t	tv_sec;
    long	tv_nsec;
};

#endif /* __SYS_TIMESPEC_H__ */

