#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "log.h"
#include "msg.h"
#include "macro.h"
#include "misc.h"

enum {
    CAT_OK,
    CAT_ERROR_INIT,
    CAT_ERROR_NOSLOT,
    CAT_ERROR_SERVER,
};

_main() {
    char buf[LOGMSGLEN];
    int nr, rc = -1;
    autoFd(clientfd);
    unixStreamClient(rc, clientfd, LOGIPC);
    if (rc == -1) return CAT_ERROR_INIT;
    msgReqInit req = {0};
    msgResInit res = {-1};
    req.role = LOG_ROLE_SUB;
    write(clientfd, &req, sizeof(req));
    read(clientfd, &res, sizeof(res));
    if (res.status == -1) return CAT_ERROR_NOSLOT;
    log1("Connect success #%d, start loop", clientfd);
    alwaysFlushOutput();
    struct pollfd pfd = {.fd=clientfd, .events=POLLIN};
    for (;;) {
        _poll(rc, poll, &pfd, 1, -1);
        if (pfd.revents & (POLLERR | POLLNVAL)) break;
        nr = (int) read(clientfd, buf, LOGMSGLEN);
        if (nr == 0) return CAT_ERROR_SERVER;
        if (nr == -1) {
            if (errno == EAGAIN || errno == EINTR) continue;
            break;
        }
        printf("%*s\n", nr, buf);
    }
    return CAT_OK;
}
