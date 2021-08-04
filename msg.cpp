#include "msg.h"

#include "base.h"
#include "log.h"

#include <unistd.h>

MAIN() {
    LOGINFO("%lu,%lu", sizeof(MsgReqHello), sizeof(MsgLog));
    LOGINFO("@log hello:%d", log::hello("logtest"));
    int i = 0;
    for (;;) {
        log::post(log::LOGLEVELINFO, "@sub #%d", ++i);
        sleep(1);
    }
    return 0;
}