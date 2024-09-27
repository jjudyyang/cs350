
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <sys/time.h>

#include <syscall.h>

#define TZ_OFFSET_SECS 0

static const char *dayOfWeek[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

time_t
time(time_t *t)
{
    uint64_t nsec = OSTime();
    time_t sec = nsec / 1000000000;

    if (t)
	*t = sec;

    return sec;
}

int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    uint64_t nsec = OSTime();

    tv->tv_sec = nsec / 1000000000;
    tv->tv_usec = (nsec % 1000000000) / 1000;

    return 0;
}

int
settimeofday(const struct timeval *tv, const struct timezone *tz)
{
    // set errno
    return -1;
}

char *
asctime_r(const struct tm *tm, char *buf)
{
    // assert(tm->tm_wday < 7);
    // assert(tm->tm_mon < 12);

    snprintf(buf, 26, "%s %s %2d %2d:%02d:%02d %4d\n",
		dayOfWeek[tm->tm_wday], months[tm->tm_mon],
		tm->tm_mday, tm->tm_hour, tm->tm_min,
		tm->tm_sec, tm->tm_year + 1900);

    return buf;
}

char *
asctime(const struct tm *tm)
{
    static char buf[26];
    return asctime_r(tm, buf);
}

char *
ctime_r(const time_t *timep, char *buf)
{
    struct tm tm;
    return asctime_r(localtime_r(timep, &tm), buf);
}

char *
ctime(const time_t *timep)
{
    return asctime(localtime(timep));
}

static bool
Time_IsLeapYear(uint64_t year)
{
    if ((year % 4) != 0)
	return false;
    if ((year % 100) != 0)
	return true;
    if ((year % 400) != 0)
	return false;
    return true;
}

static int
Time_DaysInMonth(uint64_t year, uint64_t month)
{
    static const uint64_t days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if ((month == 2) && Time_IsLeapYear(year))
	return 29;
    else
	return days[month];
}


struct tm *
gmtime_r(const time_t *timep, struct tm *tm)
{
    uint64_t secs, mins, hours;
    uint64_t days;
    uint64_t y, m;

    // Compute seconds
    secs = *timep % (60 * 60 * 24);
    days = *timep / (60 * 60 * 24);
    mins = secs / 60;
    secs = secs % 60;

    // Compute minutes
    hours = mins / 60;
    mins = mins % 60;

    // Compute hours
    hours = hours % 24;

    tm->tm_sec = secs;
    tm->tm_min = mins;
    tm->tm_hour = hours;

    tm->tm_wday = (days + 3) % 7;

    for (y = 1970; ; y++) {
	uint64_t daysOfYear;
	if (Time_IsLeapYear(y)) {
	    daysOfYear = 366;
	} else {
	    daysOfYear = 365;
	}

	if (days < daysOfYear) {
	    tm->tm_yday = days;
	    tm->tm_year = y - 1900;
	    break;
	}
	days -= daysOfYear;
    }

    for (m = 0; ; m++) {
	uint64_t daysOfMonth = Time_DaysInMonth(tm->tm_year + 1900, m);

	if (days < daysOfMonth) {
	    tm->tm_mday = days;
	    tm->tm_mon = m;
	    break;
	}
	days -= daysOfMonth;
    }

    return tm;
}

struct tm *
gmtime(const time_t *timep)
{
    static struct tm tm;
    return gmtime_r(timep, &tm);
}

struct tm *
localtime_r(const time_t *timep, struct tm *result)
{
    time_t t = *timep - TZ_OFFSET_SECS;
    return gmtime_r(&t, result);
}

struct tm *
localtime(const time_t *timep)
{
    static struct tm tm;
    return localtime_r(timep, &tm);
}

time_t
mktime(struct tm *tm)
{
    uint64_t days = 0;
    uint64_t secs = 0;
    uint64_t y, m;

    // Convert to UNIX epoch
    for (y = 70; y < tm->tm_year; y++) {
	if (Time_IsLeapYear(y))
	    days += 366;
	else
	    days += 365;
    }

    uint64_t yday = 0;
    for (m = 0; m < tm->tm_mon; m++) {
	yday += Time_DaysInMonth(tm->tm_year + 1900, m);
    }
    yday += tm->tm_mday;
    days += yday;

    secs = 24 * days + tm->tm_hour;
    secs = secs * 60 + tm->tm_min;
    secs = secs * 60 + tm->tm_sec;

    secs += TZ_OFFSET_SECS;

    return secs;
}

