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

static __thread char buf[LOGMSGLEN];
static int clientfd = STDOUT_FILENO;

int logInit(const char *tag) {
    int rc = -1;
    autoFd(fd);
    unixStreamClient(rc, fd, LOGIPC);
    if (rc == -1) return 0;

    int len = (int) strlen(tag);
    msgReqInit *reqptr = (msgReqInit *) buf;
    msgResInit res = {-1};
    reqptr->pid = myPid();
    reqptr->role = LOG_ROLE_PUB;
    reqptr->len = len;
    memcpy(reqptr->tag, tag, len);
    write(fd, buf, sizeof(*reqptr) + len);
    read(fd, &res, sizeof(res));
    if (res.status != 0) return 0;
    clientfd = fd;
    fd = -1;
    return 1;
}

int logPost(int level, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    if (clientfd == STDOUT_FILENO) {
        vfprintf(stdout, fmt, va);
        va_end(va);
        return 0;
    }
    msgLog *log = (msgLog *) buf;
    now(&log->sec, &log->us);
    log->tid = myTid();
    log->level = level;
    int n = vsnprintf(log->data, LOGBUFLEN - sizeof(*log), fmt, va);
    va_end(va);
    n += sizeof(*log);
    if (n > LOGBUFLEN) n = LOGBUFLEN;
    log->len = n - (int) sizeof(*log);
    if (write(clientfd, buf, n) == n) return 1;
    clientfd = STDOUT_FILENO;
    fprintf(stdout, "%.*s", log->len, log->data);
    return 0;
}
