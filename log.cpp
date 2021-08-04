#include "log.h"

#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

#include "msg.h"
#include "base.h"

namespace log {

static char buf[LOGMSGLEN];
static int clientfd = STDOUT_FILENO;

static void now(int &sec, int &us) {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    sec = (int) ts.tv_sec;
    us = (int) ts.tv_nsec / 1000;
}

int hello(const char *tag) {
    int fd;
    UNIXSTREAMCLIENT(fd, LOGIPC);
    UNEXPECT(fd == -1, fd);
    int l = (int) strlen(tag);
    MsgResHello res{-1};
    auto req = (MsgReqHello *) buf;
    req->type = LOGPEERPUB;
    req->pid = _getpid();
    req->len = l;
    memcpy(MSGREQSTR(req), tag, l);
    write(fd, buf, l + sizeof(*req));
    read(fd, &res, sizeof(res));
    UNEXPECT(res.status == -1, -1);
    clientfd = fd;
    _op.cancel();
    return res.status;
}

bool post(int level, const char *fmt, ...) {
    va_list va;
    if (clientfd == STDOUT_FILENO) {
        va_start(va, fmt);
        vfprintf(stdout, fmt, va);
        va_end(va);
        return false;
    }
    auto ml = (MsgLog *) buf;
    now(ml->sec, ml->us);
    ml->tid = _gettid();
    ml->level = level;
    va_start(va, fmt);
    int n = vsnprintf(buf + sizeof(*ml), LOGBUFLEN - sizeof(*ml), fmt, va);
    va_end(va);
    n += sizeof(*ml);
    n = n < LOGBUFLEN ? n : LOGBUFLEN;
    ml->len = n - (int) sizeof(*ml);
    return write(clientfd, buf, n) == n || (clientfd = STDOUT_FILENO, false);
}

}