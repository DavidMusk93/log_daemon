#include <poll.h>
#include <unistd.h>

#include "base.h"
#include "log.h"
#include "msg.h"

enum {
    ECOK,
    ECHELLO,
    ECNOSLOT,
};

MAIN() {
    char buf[LOGMSGLEN];
    int nr, rc, clientfd;
    UNIXSTREAMCLIENT(clientfd, LOGIPC);
    UNEXPECT(clientfd == -1, ECHELLO);
    MsgReqHello req{};
    MsgResHello res{};
    req.type = LOGPEERSUB;
    write(clientfd, &req, sizeof(req));
    read(clientfd, &res, sizeof(res));
    UNEXPECT(res.status == -1, ECNOSLOT);
    LOGDEBUG("connect success(%d),start loop", res.status);
    SETNOBUF();
    struct pollfd pfd{};
    SETPFD(pfd, clientfd, POLLIN);
    for (;;) {
        POLL(rc, poll, &pfd, 1, -1);
        nr = (int) read(clientfd, buf, LOGMSGLEN);
        CONTINUEIF(nr == -1 && (errno == EAGAIN || errno == EINTR));
        printf("%*s\n", nr, buf);
    }
    return ECOK;
}