#ifndef LOGDAEMON_MISC_H
#define LOGDAEMON_MISC_H

#include "defs.h"

void closeFd(void *ptrFd);
void closeFp(void *ptrFp);
void freePointer(void *ptrPtr);

int myPid();
int myTid();

void now(unsigned *secptr, unsigned *usptr);
int sleepMs(int ms);

struct sv {
    cstr s, e;
};
int split(cstr s, cstr e, char d, struct sv *svArr, int svArrLen);

#endif //LOGDAEMON_MISC_H
