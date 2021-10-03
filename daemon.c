#define _GNU_SOURCE

#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>

#include "macro.h"
#include "log.h"
#include "pubsub.h"
#include "misc.h"

static const char *logLevel2String(int level) {
#define __case(x) case LOG_LEVEL_##x:return #x
    switch (level) {
        __case(DEBUG);
        __case(INFO);
        __case(WARN);
        __case(ERROR);
        default:
            return "UNKNOWN";
    }
#undef __case
}

int specificRead(int fd, char *buf, int len) {
    int rc, retry;
    struct pollfd pfd = {.fd=fd, .events=POLLIN};
    for (retry = 0; len > 0 && retry < LOGRECVMAXRETRY; ++retry) {
        _poll(rc, poll, &pfd, 1, LOGMSGRECVTO);
        if (rc == 0) continue;
        rc = (int) read(fd, buf, len);
        if (rc == 0) break;
        if (rc == -1 && (errno == EAGAIN || errno == EINTR)) continue;
        buf += rc;
        len -= rc;
    }
    return len == 0;
}

int limitRead(int fd, int hdrlen, char *buf, int buflen) {
    int left = 0, datalen;
    int *lenlink;
    if (!specificRead(fd, buf, hdrlen)) return 0;
    datalen = *(int *) buf;
    lenlink = (int *) buf;
    if (datalen == 0) return 1;
    buf += hdrlen;
    buflen -= hdrlen;
    if (!specificRead(fd, buf, datalen < buflen ? datalen : buflen)) return 0;
    /* Truncate message if 'datalen' greater than 'buflen'-'hdrlen'. */
    if (datalen > buflen) {
        *lenlink = buflen;
        left = datalen - buflen;
        char a[512];
        do {
            if (!specificRead(fd, a, left < sizeof(a) ? left : (int) sizeof(a))) break;
            if (left <= sizeof(a)) left = 0;
            else left -= sizeof(a);
        } while (left);
    }
    return left == 0;
}

enum {
    DAEMON_OK,
    DAEMON_ERROR_SERVER,
    DAEMON_ERROR_ACCEPT,
    DAEMON_ERROR_INIT,
};

#define DAEMON_MAX_EVENTS 1024

_main() {
    signal(SIGPIPE, SIG_IGN);
    int i, rc = -1;
    autoFd(serverfd);
    autoFd(epollfd);
    struct epoll_event events[DAEMON_MAX_EVENTS], ev;
    peerManager pm;
    char buf[LOGBUFLEN];
    char msgbuf[LOGMSGLEN];
    msgReqInit *reqinit = (msgReqInit *) buf;
    msgLog *log = (msgLog *) buf;
    pubEntry se;

    unixStreamServer(rc, serverfd, LOGIPC);
    if (rc == -1) return DAEMON_ERROR_SERVER;
    se.fd = serverfd;
    epollfd = epoll_create1(0);
    memset(&ev, 0, sizeof(ev));
    ev.data.ptr = &se;
    ev.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, serverfd, &ev);
    pmInit(&pm);

    for (;;) {
        _poll(rc, epoll_wait, epollfd, events, DAEMON_MAX_EVENTS, -1);
        for (i = 0; i < rc; i++) {
            int fd = ((pubEntry *) events[i].data.ptr)->fd;
            if (fd == serverfd) {
                autoFd(peerfd);
                peerfd = accept4(fd, 0, 0, O_NONBLOCK);
                if (peerfd == -1) return DAEMON_ERROR_ACCEPT;
                if (!limitRead(peerfd, sizeof(msgReqInit), buf, LOGBUFLEN)) {
                    log2("Error occurs on peer init");
                    continue;
                }
                if (reqinit->role == LOG_ROLE_PUB) {
                    ev.data.ptr = pmNewPub(&pm, peerfd, reqinit);
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, peerfd, &ev);
                    log1("New publisher #%d", peerfd);
                } else if (reqinit->role == LOG_ROLE_SUB) {
                    pmNewSub(&pm, peerfd);
                    log1("New subscriber #%d", peerfd);
                }
                msgResInit resinit = {0};
                write(peerfd, &resinit, sizeof resinit);
                peerfd = -1;
            } else {
                pubEntry *entry = events[i].data.ptr;
                if (events[i].events & (EPOLLERR | EPOLLHUP) || !limitRead(fd, sizeof(msgLog), buf, LOGBUFLEN)) {
                    pmFreePub(&pm, entry);
                    log1("Publisher #%d leave", fd);
                } else {
                    int n = sprintf(msgbuf, "%u.%06u %d#%d %*s %s %*s",
                                    log->sec, log->us,
                                    entry->pid, log->tid,
                                    entry->len, entry->tag,
                                    logLevel2String(log->level),
                                    log->len, log->data);
                    pmPost(&pm, msgbuf, n);
                }
            }
        }
    }
    pmFree(&pm);
    return 0;
}