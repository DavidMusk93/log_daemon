#ifndef LOGDAEMON_MISC_H
#define LOGDAEMON_MISC_H

void closeFd(void *arg);
void closeFp(void *arg);

int myPid();
int myTid();

void now(unsigned *secptr, unsigned *usptr);

#endif //LOGDAEMON_MISC_H
