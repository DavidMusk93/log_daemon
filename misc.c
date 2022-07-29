#include "misc.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <string.h>

#include "macro.h"

void closeFd(void *ptrFd) {
    int fd = *(int *) ptrFd;
    if (fd != -1) close(fd);
}

void closeFp(void *ptrFp) {
    FILE *fp = *(FILE **) ptrFp;
    if (fp) fclose(fp);
}

void freePointer(void *ptrPtr) {
    void *ptr = *(void **) ptrPtr;
    if (ptr) {
        free(ptr);
    }
}

static int processId;
static __thread int threadId;

__ctor() {
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

int sleepMs(int ms) {
    return poll(NULL, 0, ms);
}

int split(cstr s, cstr e, char d, struct sv *svArr, int svArrLen) {
    int i;
    cstr p;
    for (i = 0; s < e; s = p + 1) {
        p = memchr(s, d, e - s);
        if (!p) {
            break;
        }
        svArr[i++] = (struct sv) {s, p};
        if (i == svArrLen) {
            return i;
        }
    }
    svArr[i++] = (struct sv) {s, e};
    return i;
}
