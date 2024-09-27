
#ifndef __SYS_KTIME_H__
#define __SYS_KTIME_H__

uint64_t Time_GetTSC();

typedef struct KTime {
    int		sec;
    int		min;
    int		hour;
    int		month;
    int		year;
    int		mday;
    int		wday;
    int		yday;
} KTime;

typedef uint64_t UnixEpoch;
typedef uint64_t UnixEpochNS;

void KTime_Fixup(KTime *tm);
UnixEpoch KTime_ToEpoch(const KTime *tm);
void KTime_FromEpoch(UnixEpoch time, KTime *tm);
void KTime_SetTime(UnixEpoch epoch, uint64_t tsc, uint64_t tps);
void KTime_Tick(int rate);
UnixEpoch KTime_GetEpoch();
UnixEpochNS KTime_GetEpochNS();

#endif /* __SYS_KTIME_H__ */

