#include "misc.h"

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include "macro.h"

void closeFd(void *arg) {
    int fd = *(int *) arg;
    if (fd != -1) close(fd);
}

void closeFp(void *arg) {
    FILE *fp = *(FILE **) arg;
    if (fp) fclose(fp);
}

static int processId;
static __thread int threadId;

static attrCtor void initGlobalVariables() {
    processId = getpid();
}

int myPid() {
    return processId;
}

int myTid() {
    if (!threadId) {
        threadId = (int) syscall(__NR_gettid);
    }
    return threadId;
}

void now(unsigned *secptr, unsigned *usptr) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    *secptr = tv.tv_sec;
    *usptr = tv.tv_usec;
}
