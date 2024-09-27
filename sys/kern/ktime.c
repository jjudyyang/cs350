/*
 * Copyright (c) 2006-2022 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/ktime.h>
#include <sys/spinlock.h>

static Spinlock ktimeLock;
static uint64_t ktimeLastEpoch;
static uint64_t ktimeLastTSC;
uint64_t ticksPerSecond;

static const char *dayOfWeek[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

void
KTime_Init()
{
    Spinlock_Init(&ktimeLock, "KTime Lock", SPINLOCK_TYPE_NORMAL);
    ktimeLastEpoch = 0;
    ktimeLastTSC = 0;
    ticksPerSecond = 0;
}

static bool
KTimeIsLeapYear(uint64_t year)
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
KTimeDaysInMonth(uint64_t year, uint64_t month)
{
    static const uint64_t days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if ((month == 2) && KTimeIsLeapYear(year))
	return 29;
    else
	return days[month];
}

/*
 * This function recomputes yday, mday or wday given other fields
 */
void
KTime_Fixup(KTime *tm)
{
    uint64_t m;

    if (tm->yday == -1) {
	uint64_t yday = 0;
	for (m = 0; m < tm->month; m++) {
	    yday += KTimeDaysInMonth(tm->year, m);
	}
	yday += tm->mday;
	tm->yday = yday;
    }
}

UnixEpoch
KTime_ToEpoch(const KTime *tm)
{
    uint64_t days = 0;
    uint64_t secs = 0;
    uint64_t y, m;

    // Convert to UNIX epoch
    for (y = 1970; y < tm->year; y++) {
	if (KTimeIsLeapYear(y))
	    days += 366;
	else
	    days += 365;
    }

    if (tm->yday == -1) {
	uint64_t yday = 0;
	for (m = 0; m < tm->month; m++) {
	    yday += KTimeDaysInMonth(tm->year, m);
	}
	yday += tm->mday;
	days += yday;
    } else {
	days += tm->yday;
    }

    secs = 24 * days + tm->hour;
    secs = secs * 60 + tm->min;
    secs = secs * 60 + tm->sec;

    return secs;
}

void
KTime_FromEpoch(UnixEpoch epoch, KTime *tm)
{
    uint64_t secs, mins, hours;
    uint64_t days;
    uint64_t y, m;

    // Compute seconds
    secs = epoch % (60 * 60 * 24);
    days = epoch / (60 * 60 * 24);
    mins = secs / 60;
    secs = secs % 60;

    // Compute minutes
    hours = mins / 60;
    mins = mins % 60;

    // Compute hours
    hours = hours % 24;

    tm->sec = secs;
    tm->min = mins;
    tm->hour = hours;

    tm->wday = (days + 3) % 7;

    for (y = 1970; ; y++) {
	uint64_t daysOfYear;
	if (KTimeIsLeapYear(y)) {
	    daysOfYear = 366;
	} else {
	    daysOfYear = 365;
	}

	if (days < daysOfYear) {
	    tm->yday = days;
	    tm->year = y;
	    break;
	}
	days -= daysOfYear;
    }

    for (m = 0; ; m++) {
	uint64_t daysOfMonth = KTimeDaysInMonth(tm->year, m);

	if (days < daysOfMonth) {
	    tm->mday = days;
	    tm->month = m;
	    break;
	}
	days -= daysOfMonth;
    }
}

void
KTime_SetTime(UnixEpoch epoch, uint64_t tsc, uint64_t tps)
{
    Spinlock_Lock(&ktimeLock);
    ktimeLastEpoch = epoch;
    ktimeLastTSC = tsc;
    ticksPerSecond = tps;
    Spinlock_Unlock(&ktimeLock);

}

void
KTime_GetTime(KTime *tm)
{
    KTime_FromEpoch(KTime_GetEpoch(), tm);
}

UnixEpoch
KTime_GetEpoch()
{
    uint64_t tscDiff;
    uint64_t epoch;

    Spinlock_Lock(&ktimeLock);
    tscDiff = Time_GetTSC() - ktimeLastTSC;
    epoch = ktimeLastEpoch + tscDiff / ticksPerSecond;
    Spinlock_Unlock(&ktimeLock);

    return epoch;
}

UnixEpochNS
KTime_GetEpochNS()
{
    uint64_t tscDiff;
    uint64_t epoch;

    Spinlock_Lock(&ktimeLock);
    tscDiff = Time_GetTSC() - ktimeLastTSC;
    /*
     * This is ugly but it avoids overflowing tscDiff to time computation.  
     * Note that the bottom bits of ticksPerSecond are not significant so it is 
     * okay to discard them.
     */
    epoch = (ktimeLastEpoch * 1000000000ULL) + (tscDiff * 1000000ULL / ticksPerSecond * 1000ULL);
    Spinlock_Unlock(&ktimeLock);

    return epoch;
}

static void
Debug_Date()
{
    UnixEpoch epoch = KTime_GetEpoch();
    KTime tm;

    KTime_FromEpoch(epoch, &tm);

    kprintf("%s %s %d %02d:%02d:%02d %04d\n",
	    dayOfWeek[tm.wday], months[tm.month],
	    tm.mday, tm.hour, tm.min, tm.sec, tm.year);

    kprintf("Epoch: %lu\n", epoch);
}

REGISTER_DBGCMD(date, "Print date", Debug_Date);

static void
Debug_Ticks()
{
    kprintf("Ticks Per Second: %lu\n", ticksPerSecond);
}

REGISTER_DBGCMD(ticks, "Print ticks per second", Debug_Ticks);

