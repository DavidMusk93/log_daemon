#include <unordered_map>
#include <functional>
#include <vector>

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>

#include "base.h"
#include "log.h"
#include "msg.h"

class IdMgr {
#define IDMAXLEN 64
public:
    IdMgr() : x_(0), c_(0) {}

    int get() {
        UNEXPECT(c_ == IDMAXLEN, -1);
        for (u64 i = 0, m; i < IDMAXLEN; i++) {
            m = 1UL << i;
            if (!(x_ & m)) {
                c_++;
                x_ |= m;
                return (int) i;
            }
        }
        return -1;
    }

    void put(int i) {
        if (i >= 0 && i < IDMAXLEN) {
            c_--;
            x_ &= ~(1UL << i);
        }
    }

    int count() const { return c_; }

private:
    u64 x_;
    int c_;
};

static const char *loglevelstr(int level) {
    switch (level) {
#define CASE(x) case log::LOGLEVEL##x:return #x
        CASE(DEBUG);
        CASE(INFO);
        CASE(WARN);
        CASE(ERROR);
        default:
            return "UNKNOWN";
#undef CASE
    }
}

enum {
    ENTRYFREE = (1 << 0),
    ENTRYBUSY = (1 << 1),
};

class PubMgr;

class SubMgr {
public:
    struct entry {
        int flag;
        int fd;
        void **interests;
    };

    SubMgr() : im_{}, es_{} {}

    ~SubMgr() {
        for (auto &&t:es_) {
            free(t);
        }
    }

    int newsub(int fd) {
        int i = im_.get();
        UNEXPECT(i == -1, i);
        auto &e = es_[i];
        if (!e) {
            e = (entry *) malloc(sizeof(entry));
        }
        e->flag = ENTRYBUSY;
        e->fd = fd;
        m_[fd] = i;
        return i;
    }

    void deletesub(int fd) {
        LOGWARN("drop sub(#%d)", fd);
        int i = m_[fd];
        im_.put(i);
        es_[i]->flag = ENTRYFREE;
        close(fd);
    }

private:
    IdMgr im_;
    entry *es_[IDMAXLEN];
    std::unordered_map<int, int> m_;

    friend class PubMgr;
};

class PubMgr {
public:
    struct entry {
        int pid;
        int len;
//        char *tag;
#define PUBENTRYSTR(p) ((char*)(p)+sizeof(entry))
    };

    PubMgr() : im_{}, es_{} {}

    ~PubMgr() {
        for (auto &&t:es_) {
            free(t);
        }
    }

    int newpub(int fd, MsgReqHello *req) {
        int i = im_.get();
        UNEXPECT(i == -1, i);
        auto &e = es_[i];
//        if (!e) {
//            e = (entry *) malloc(sizeof(entry) + req->len);
//        } else if (e->len < req->len) {
//            e = (entry *) realloc(e, sizeof(entry) + req->len);
//        }
        if (!e || e->len < req->len) {
            e = (entry *) realloc(e, sizeof(entry) + req->len);
        }
        e->pid = req->pid;
        e->len = req->len;
        memcpy(PUBENTRYSTR(e), MSGREQSTR(req), req->len);
        m_[fd] = i;
        return i;
    }

    void deletepub(int fd) {
        LOGWARN("drop pub(#%d)", fd);
        im_.put(m_[fd]);
        close(fd);
    }

    void pubmsg(int fd, MsgLog *ml, char msgbuf[LOGMSGLEN]/*large enough*/, SubMgr *sm) {
        int c1, c2, c3;
        c1 = sm->im_.count();
        if (!c1) {
            LOGINFO("no sub, drop msg");
            return;
        }
        c2 = c1, c3 = 0;
        auto &&e = es_[m_[fd]];
        int n = sprintf(msgbuf, "%d.%06d %d#%d %*s %s %*s",
                        ml->sec, ml->us,
                        e->pid, ml->tid,
                        e->len, PUBENTRYSTR(e),
                        loglevelstr(ml->level),
                        ml->len, MSGLOGSTR(ml));
        for (auto t : sm->es_) {
            if (t && (t->flag & ENTRYBUSY) &&
                (/*({ LOGDEBUG("pub(%d) msg(%d) to sub(%d)", fd, n, t->fd); }),*/c2--, write(t->fd, msgbuf, n) != n)) {
                c3++;
                sm->deletesub(t->fd);
            }
            BREAKIF(!c2);
        }
        LOGINFO("pub(#%d) post summarize: %d success, %d failure", fd, c1 - c3, c3);
    }

private:
    IdMgr im_;
    entry *es_[IDMAXLEN];
    std::unordered_map<int, int> m_;
};

enum {
    ECOK,
    ECNEWSERVER,
    ECACCEPT,
    ECHELLO,
};

static bool readmsg(int fd, char *buf, int buflen/*specified*/) {
    int nr, retry;
    struct pollfd pfd{};
    pfd.fd = fd, pfd.events = POLLIN;
    for (retry = 0; buflen > 0 && retry < LOGRECVMAXRETRY; ++retry) {
        POLL(nr, poll, &pfd, 1, LOGMSGRECVTO);
        CONTINUEIF(nr == 0);
        nr = (int) read(fd, buf, buflen);
        BREAKIF(nr == 0); /*eof*/
        CONTINUEIF(nr == -1 && (errno == EAGAIN || errno == EINTR));
        buf += nr;
        buflen -= nr;
    }
    return !buflen;
}

static bool readmsg(int fd, int hdrlen, char *buf, int buflen/*not sure*/) {
    int rc = true;
    UNEXPECT(!readmsg(fd, buf, hdrlen), false);
    int msglen = ((MsgStr *) buf)->len;
    UNEXPECT(msglen == 0, true);
    buf += hdrlen;
    buflen -= hdrlen;
    defer op;
    if (msglen > buflen) {
        LOGWARN("truncate msg");
        op = defer([&] {
            int left = msglen - buflen;
            msglen = buflen;
            char a[512];
            do {
                BREAKIF(!readmsg(fd, a, left < (int) sizeof(a) ? left : sizeof(a)));
                if (left <= (int) sizeof(a)) {
                    left = 0;
                } else {
                    left -= sizeof(a);
                }
            } while (left);
            rc = !left;
        });
    }
    rc = readmsg(fd, buf, msglen < buflen ? msglen : buflen);
    op();
    return rc;
}

MAIN() {
    signal(SIGPIPE, SIG_IGN);
    int serverfd, rc, status;
    int peerfd;
    UNIXSTREAMSERVER(serverfd, LOGIPC);
    UNEXPECT(serverfd == -1, ECNEWSERVER);
    LOGINFO("@server address:%s", LOGIPC);
    struct pollfd pfds[2 + IDMAXLEN]{};
    EventFd ctl{};
    int np{};
    union {
        double _t; /*force align?*/
        char buf[LOGBUFLEN];
    };
    char msgbuf[LOGMSGLEN];
    struct pollfd *sp = &pfds[2];
    auto req = (MsgReqHello *) buf;
    auto ml = (MsgLog *) buf;
    PubMgr pm;
    SubMgr sm;
    SETPFD(pfds[0], ctl.fd(), POLLIN);
    SETPFD(pfds[1], serverfd, POLLIN);
    for (;;) {
        POLL(rc, poll, pfds, 2 + np, -1);
        if (pfds[0].revents & POLLIN) {
            rc--;
            auto e = ctl.wait();
            BREAKIF(e >= EventFd::kQuit);
            CONTINUEIF(!rc);
        }
        if (pfds[1].revents & POLLIN) {
            rc--;
            peerfd = accept4(serverfd, 0, 0, O_NONBLOCK);
            defer op([peerfd] {
                LOGWARN("drop peer");
                close(peerfd);
            });
            UNEXPECT(peerfd == -1, ECACCEPT);
            status = -1;
            if (!readmsg(peerfd, sizeof(MsgReqHello), buf, LOGBUFLEN)) {
                LOGWARN("hello error");
            } else if (req->type == LOGPEERPUB) { /*pub*/
                status = pm.newpub(peerfd, req);
                LOGINFO("new pub #%d", status);
                if (status != -1) {
                    auto &pfd = sp[np++];
                    pfd.fd = peerfd, pfd.events = POLLIN;
                    op.cancel();
                }
                // no slot left
            } else if (req->type == LOGPEERSUB) { /*sub*/
                status = sm.newsub(peerfd);
                LOGINFO("new sub #%d", status);
                if (status != -1) {
                    op.cancel();
                }
            }
            MsgResHello res{status};
            write(peerfd, &res, sizeof(res));
            CONTINUEIF(!rc);
        }
        for (int i = 0; i < np;) {
            auto &pfd = sp[i];
            if (pfd.revents & (POLLIN | POLLNVAL)) { /*ready or error*/
                rc--;
                if ((pfd.revents & POLLIN) && readmsg(pfd.fd, sizeof(MsgLog), buf, LOGBUFLEN)) {
//                    LOGDEBUG("pub(%d) recv success", pfd.fd);
                    pm.pubmsg(pfd.fd, ml, msgbuf, &sm);
                    i++;
                } else {
                    LOGDEBUG("pub(#%d) may quit", pfd.fd);
                    pm.deletepub(pfd.fd);
                    np--;
                    if (i != np) {
                        auto t = sp[i];
                        sp[i] = sp[np];
                        sp[np] = t;
                    }
                }
                BREAKIF(!rc);
            }
        }
    }
}
