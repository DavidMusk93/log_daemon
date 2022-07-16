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
#define CLIENT_READ_MAX_RETRY 5

_main() {
    char buf[LOGMSGLEN];
    int charCount, rc = -1;
    int readRetry = 0;
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
        charCount = (int) read(clientfd, buf, LOGMSGLEN);
        if (charCount == 0) {
            if (readRetry++ < CLIENT_READ_MAX_RETRY) {
                sleepMs(readRetry * 200);
                continue;
            }
            return CAT_ERROR_SERVER;
        }
        readRetry = 0;
        if (charCount == -1) {
            if (errno == EAGAIN || errno == EINTR) continue;
            break;
        }
        /* The message maybe merged, the log daemon will control the newline. */
        printf("%.*s", charCount, buf);
    }
    return CAT_OK;
}
