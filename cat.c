#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>

#include "log.h"
#include "msg.h"
#include "macro.h"
#include "misc.h"
#include "packer.h"

enum {
    CAT_OK,
    CAT_ERROR_INIT,
    CAT_ERROR_NOSLOT,
    CAT_ERROR_SERVER,
    CAT_ARG_MISS_VALUE,
    CAT_ARG_UNKNOWN_OPTION,
};
#define CLIENT_READ_MAX_RETRY 5

mainEx(argc, argv) {
    int opt, rc;
    const int tagCountMax = 10;
    struct sv svArr[tagCountMax];
    cstr tagStr = NULL;

    rc = CAT_OK;
    while ((opt = getopt(argc, argv, ":t:h")) != -1) {
        switch (opt) {
            case 't':
                tagStr = optarg;
                break;
            case ':':
                rc = CAT_ARG_MISS_VALUE;
                goto usage;
            case '?':
                rc = CAT_ARG_UNKNOWN_OPTION;
                goto usage;
            case 'h':
            usage:
                log2("Usage: %s\n"
                     "  -t tag1,tag2,...\n"
                     "        set tag list(<=%d) for filter",
                     argv[0], tagCountMax);
                return rc;
            default:
                __builtin_unreachable();
        }
    }

    int charCount;
    int readRetry = 0;
    initLogRequest *ptrReq;

    autoFd(clientFd);
    unixStreamClient(rc, clientFd, LOGIPC);
    if (rc == -1)
        return CAT_ERROR_INIT;

    const int recvBufSize = 16 * 1024 * 1024;
    char *recvBuf attrScopeGuard(freePointer) = malloc(recvBufSize);
    setsockopt(clientFd, SOL_SOCKET, SO_RCVBUF, &recvBufSize, sizeof(recvBufSize));

    struct packerFilter packer attrScopeGuard(freePackerFilter) = {};
    int sizeReq;
    int i, tagCount = 0;
    if (tagStr) {
        tagCount = split(tagStr, tagStr + strlen(tagStr), ',', svArr, dimensionOf(svArr));
    }
    initPackerFilter(&packer);
    for (i = 0; i < tagCount; i++) {
        packTag(&packer, svArr[i].s, (int) (svArr[i].e - svArr[i].s));
    }

    ptrReq = finalizePackerFilter(&packer, &sizeReq);
    ptrReq->len = sizeReq - (int) sizeof(*ptrReq);
    ptrReq->role = LOG_ROLE_SUB;

    initLogResponse res = {-1};

    write(clientFd, ptrReq, sizeReq);
    read(clientFd, &res, sizeof(res));

    if (res.status == -1) return CAT_ERROR_NOSLOT;
    log1("Connect success #%d, start loop", clientFd);

    alwaysFlushOutput();
    struct pollfd pfd = {.fd=clientFd, .events=POLLIN};

    for (;;) {
        _poll(rc, poll, &pfd, 1, -1);
        if (pfd.revents & (POLLERR | POLLNVAL)) break;
        charCount = (int) read(clientFd, recvBuf, recvBufSize);
        if (charCount == 0) {
            if (readRetry++ < CLIENT_READ_MAX_RETRY) { /* may be trivial? */
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
        printf("%.*s", charCount, recvBuf);
    }
    return CAT_OK;
}
