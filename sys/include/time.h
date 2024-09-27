
#ifndef __SYS_TIME_H__
#define __SYS_TIME_H__

#include <sys/types.h>

struct timeval
{
    time_t	tv_sec;
    suseconds_t	tv_usec;
};

struct timezone {
    int		tz_minuteswest;
    int		tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);

// XXX: Not implemented
int settimeofday(const struct timeval *tv, const struct timezone *tz);

#endif /* __SYS_TIME_H__ */

