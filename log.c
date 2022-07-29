#include "log.h"

#include <unistd.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>

#include "msg.h"
#include "misc.h"
#include "macro.h"

static __thread char sendBuf[LOGMSGLEN];
static __thread int clientFd = STDOUT_FILENO;

__dtor() {
    if (clientFd != STDOUT_FILENO) {
        closeFd(&clientFd);
    }
}

int logInit(const char *tag) {
    int rc = -1;
    autoFd(fd);
    unixStreamClient(rc, fd, LOGIPC);
    if (rc == -1) return 0;

    int len = (int) strlen(tag);
    initLogRequest *ptrReq = (initLogRequest *) sendBuf;
    initLogResponse res = {-1};
    ptrReq->pid = myPid();
    ptrReq->role = LOG_ROLE_PUB;
    ptrReq->len = len;
    memcpy(ptrReq->tag, tag, len);
    write(fd, sendBuf, sizeof(*ptrReq) + len);
    read(fd, &res, sizeof(res));
    if (res.status != 0) return 0;
    clientFd = fd;
    fd = -1;
    return 1;
}

int logPost(int level, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    if (clientFd == STDOUT_FILENO) {
        vfprintf(stdout, fmt, va);
        va_end(va);
        return 0;
    }
    msgLog *log = (msgLog *) sendBuf;
    now(&log->sec, &log->us);
    log->tid = myTid();
    log->level = level;
    int n = vsnprintf(log->data, LOGBUFLEN - sizeof(*log), fmt, va);
    va_end(va);
    n += sizeof(*log);
    if (n > LOGBUFLEN) n = LOGBUFLEN;
    log->len = n - (int) sizeof(*log);
    if (write(clientFd, sendBuf, n) == n) /* make sure atomic write */ return 1;
    clientFd = STDOUT_FILENO;
    fprintf(stdout, "%.*s", log->len, log->data);
    return 0;
}
