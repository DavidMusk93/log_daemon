#include "misc.h"

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/time.h>

void closeFd(void *arg) {
    int fd = *(int *) arg;
    if (fd != -1) close(fd);
}

void closeFp(void *arg) {
    FILE *fp = *(FILE **) arg;
    if (fp) fclose(fp);
}

int myPid() {
    static int pid;
    if (!pid) {
        pid = getpid();
    }
    return pid;
}

int myTid() {
    static __thread int tid;
    if (!tid) {
        tid = (int) syscall(__NR_gettid);
    }
    return tid;
}

void now(unsigned *secptr, unsigned *usptr) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    *secptr = tv.tv_sec;
    *usptr = tv.tv_usec;
}
