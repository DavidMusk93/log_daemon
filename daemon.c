#define _GNU_SOURCE

#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>

#include <signal.h>
#include <sys/signalfd.h>

#include "macro.h"
#include "log.h"
#include "pubsub.h"
#include "misc.h"
#include "object.h"

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
    DAEMON_ERROR_SIGNALFD,
    DAEMON_ERROR_INIT,
};

#define DAEMON_MAX_EVENTS 1024

_main() {
    signal(SIGPIPE, SIG_IGN);
    int i, rc = -1;
    const int subscriberSendBufSize = 16 * 1024 * 1024;
    autoFd(serverFd);
    autoFd(epollFd);
    struct epoll_event events[DAEMON_MAX_EVENTS], ev;
    peerManager manager;
    _attr(aligned(8)) char protocolBuffer[LOGBUFLEN];
    msgReqInit *initRequest = (msgReqInit *) protocolBuffer;
    msgLog *log = (msgLog *) protocolBuffer;
    pubEntry serverEntry;
    struct message msg;

    pubEntry signalEntry;
    sigset_t signalMask;
    int signalFd;
    struct signalfd_siginfo signalInfo;

    epollFd = epoll_create1(0);
    memset(&ev, 0, sizeof(ev));

    sigemptyset(&signalMask);
    sigaddset(&signalMask, SIGINT);
    sigaddset(&signalMask, SIGQUIT);
    sigprocmask(SIG_BLOCK, &signalMask, NULL);
    signalFd = signalfd(-1, &signalMask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signalFd == -1) {
        return DAEMON_ERROR_SIGNALFD;
    }
    signalEntry.fd = signalFd;
    ev.data.ptr = &signalEntry;
    ev.events = EPOLLIN;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, signalFd, &ev);

    unixStreamServer(rc, serverFd, LOGIPC);
    if (rc == -1) return DAEMON_ERROR_SERVER;
    serverEntry.fd = serverFd;
    ev.data.ptr = &serverEntry;
    ev.events = EPOLLIN;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &ev);
    initPeerManager(&manager);

    for (;;) {
        _poll(rc, epoll_wait, epollFd, events, DAEMON_MAX_EVENTS, -1);
        for (i = 0; i < rc; i++) {
            int fd = ((pubEntry *) events[i].data.ptr)->fd;
            if (fd == serverFd) {
                autoFd(peerFd);
                peerFd = accept4(fd, 0, 0, O_NONBLOCK);
                if (peerFd == -1) return DAEMON_ERROR_ACCEPT;
                if (!limitRead(peerFd, sizeof(msgReqInit), protocolBuffer, LOGBUFLEN)) {
                    log2("Error occurs on peer init");
                    continue;
                }
                if (initRequest->role == LOG_ROLE_PUB) {
                    ev.data.ptr = newPub(&manager, peerFd, initRequest);
                    epoll_ctl(epollFd, EPOLL_CTL_ADD, peerFd, &ev);
                    log1("New publisher #%d", peerFd);
                } else if (initRequest->role == LOG_ROLE_SUB) {
                    newSub(&manager, peerFd);
                    log1("New subscriber #%d", peerFd);
                    setsockopt(peerFd, SOL_SOCKET, SO_SNDBUF, &subscriberSendBufSize, sizeof(subscriberSendBufSize));
                }
                msgResInit initResponse = {0};
                write(peerFd, &initResponse, sizeof initResponse);
                peerFd = -1;
            } else if (__builtin_expect(fd == signalFd, 0)) {
                (void) read(signalFd, &signalInfo, sizeof(signalInfo));
                log2("recv signal:%d, quit.", signalInfo.ssi_signo);
                goto quit;
            } else {
                pubEntry *entry = events[i].data.ptr;
                if (events[i].events & (EPOLLERR | EPOLLHUP) ||
                    !limitRead(fd, sizeof(msgLog), protocolBuffer, LOGBUFLEN)) {
                    freePub(&manager, entry);
                    log1("Publisher #%d leave", fd);
                } else {
                    msg.sec = log->sec;
                    msg.us = log->us;
                    msg.pid = entry->pid;
                    msg.tid = log->tid;
                    msg.level = log->level;
                    msg.tag = refObject(entry->tag);
                    msg.content = makeObject(sizeof(struct logContent) + log->len, NULL, NULL);
                    msg.content->len = log->len;
                    memcpy(msg.content->data, log->data, log->len);
                    postMessage(&manager, &msg);
                }
            }
        }
    }
    quit:
    freePeerManager(&manager);
    return DAEMON_OK;
}
