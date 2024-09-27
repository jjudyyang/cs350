
#ifndef __CV_H__
#define __CV_H__

typedef struct CV {
    WaitChannel		chan;
} CV;

void CV_Init(CV *cv, const char *name);
void CV_Destroy(CV *cv);
void CV_Wait(CV *cv, Mutex *mtx);
void CV_Signal(CV *cv);
void CV_Broadcast(CV *cv);

#endif /* __CV_H__ */

