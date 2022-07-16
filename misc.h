#ifndef LOGDAEMON_MISC_H
#define LOGDAEMON_MISC_H

void closeFd(void *arg);
void closeFp(void *arg);

int myPid();
int myTid();

void now(unsigned *secptr, unsigned *usptr);

int sleepMs(int ms);

#endif //LOGDAEMON_MISC_H
