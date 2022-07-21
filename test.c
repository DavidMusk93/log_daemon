#include "msg.h"

#include <stdio.h>
#include <unistd.h>

#include "macro.h"
#include "log.h"
#include "misc.h"

_main() {
//    log1("%lu,%lu", sizeof(msgReqInit), sizeof(msgLog));
    log1("Log init:%d", logInit("logtest"));

    int i = 0;
    for (;;) {
        logPost(LOG_LEVEL_INFO, "posting #%d", ++i);
        sleepMs(1000);
    }
    return 0;
}