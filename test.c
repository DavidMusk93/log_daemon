#include "msg.h"

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

#include "macro.h"
#include "log.h"
#include "misc.h"

struct testOption {
    int pubInterval;
    int pubCount;
};

enum {
    TEST_OK,
    TEST_MISS_VALUE,
    TEST_UNKNOWN_OPTION,
};

mainEx(argc, argv) {
    struct testOption option = {.pubInterval=100, .pubCount=0xfffffff};
    int rc, opt;

    rc = TEST_OK;
    while ((opt = getopt(argc, argv, ":i:n:h")) != -1) {
        switch (opt) {
            case 'i':
                option.pubInterval = atoi(optarg);
                break;
            case 'n':
                option.pubCount = atoi(optarg);
                break;
            case ':':
                rc = TEST_MISS_VALUE;
                goto usage;
            case '?':
                rc = TEST_UNKNOWN_OPTION;
                goto usage;
            case 'h':
            usage:
                log2("Usage: %s\n"
                     "  -i $interval\n"
                     "        publish interval(ms)\n"
                     "  -n $count\n"
                     "        total publish count", argv[0]);
                return rc;
            default:
                __builtin_unreachable();
        }
    }

    log1("Log init:%d", logInit("logtest"));

    int i = 0;
    for (; i < option.pubCount; i++) {
        logPost(LOG_LEVEL_INFO, "posting #%d", i);
        sleepMs(option.pubInterval);
    }
    return 0;
}