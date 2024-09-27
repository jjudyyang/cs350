
#ifndef __TIME_H__
#define __TIME_H__

#include <sys/timespec.h>

struct tm {
    int		tm_sec;
    int		tm_min;
    int		tm_hour;
    int		tm_mday;
    int		tm_mon;
    int		tm_year;
    int		tm_wday;
    int		tm_yday;
    int		tm_isdst;
};

time_t time(time_t *t);
char *asctime_r(const struct tm *tm, char *buf);
char *asctime(const struct tm *tm);
char *ctime_r(const time_t *timep, char *buf);
char *ctime(const time_t *timep);
struct tm *gmtime(const time_t *timep);
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime(const time_t *timep);
struct tm *localtime_r(const time_t *timep, struct tm *result);
time_t mktime(struct tm *tm);

#endif /* __TIME_H__ */

