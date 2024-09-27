
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/ktime.h>

#include "ioport.h"

#define RTC_SECONDS	0x00
#define RTC_MINUTES	0x02
#define RTC_HOURS	0x04
#define RTC_WEEKDAY	0x06
#define RTC_DAY		0x07
#define RTC_MONTH	0x08
#define RTC_YEAR	0x09

UnixEpoch RTC_ReadTime();

void
RTC_Init()
{
    uint64_t startTSC, stopTSC;
    UnixEpoch first, second;

    kprintf("RTC: Measuring CPU clock...\n");

    first = RTC_ReadTime();
    while (1) {
	second = RTC_ReadTime();
	if (first != second)
	    break;
	first = second;
    }
    startTSC = Time_GetTSC();

    first = RTC_ReadTime();
    while (1) {
	second = RTC_ReadTime();
	if (first != second)
	    break;
	first = second;
    }
    stopTSC = Time_GetTSC();

    kprintf("RTC: %lld Ticks Per Second: %lld\n", second, stopTSC - startTSC);

    KTime_SetTime(second, stopTSC, stopTSC - startTSC);
}

static inline uint8_t
RTC_ReadReg(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

UnixEpoch
RTC_ReadTime()
{
    KTime tm;
    bool isPM = false;
    uint8_t flags = RTC_ReadReg(0x0B);

    // Read RTC
    tm.sec = RTC_ReadReg(RTC_SECONDS);
    tm.min = RTC_ReadReg(RTC_MINUTES);
    tm.hour = RTC_ReadReg(RTC_HOURS);
    tm.wday = RTC_ReadReg(RTC_WEEKDAY);
    tm.mday = RTC_ReadReg(RTC_DAY);
    tm.month = RTC_ReadReg(RTC_MONTH);
    tm.year = RTC_ReadReg(RTC_YEAR);

    // Convert BCD & 24-hour checks
    if (tm.hour & 0x80) {
	isPM = true;
    }
    if ((flags & 0x04) == 0) {
#define BCD_TO_BIN(_B) ((_B & 0x0F) + ((_B >> 4) * 10))
	tm.sec = BCD_TO_BIN(tm.sec);
	tm.min = BCD_TO_BIN(tm.min);
	tm.hour = BCD_TO_BIN((tm.hour & 0x7F));
	tm.wday = BCD_TO_BIN(tm.wday);
	tm.mday = BCD_TO_BIN(tm.mday);
	tm.month = BCD_TO_BIN(tm.month);
	tm.year = BCD_TO_BIN(tm.year);
    }
    if (((flags & 0x02) == 0) && isPM) {
	tm.hour = (tm.hour + 12) % 24;
    }

    tm.year += 2000;
    tm.yday = -1;

    tm.wday -= 1;
    tm.month -= 1;

//    KTime_SetTime(&tm);
    return KTime_ToEpoch(&tm);
}

