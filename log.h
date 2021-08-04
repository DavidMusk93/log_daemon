#ifndef LOG_DAEMON_LOG_H
#define LOG_DAEMON_LOG_H

#define LOGIPC "/tmp/log.ipc"
#define LOGBUFLEN 2048
#define LOGMSGLEN 4096

#define LOGMSGRECVTO 500 /*milliseconds*/
#define LOGRECVMAXRETRY 3

namespace log {

enum {
    LOGLEVELDEBUG,
    LOGLEVELINFO,
    LOGLEVELWARN,
    LOGLEVELERROR,
};

int hello(const char *tag);

bool post(int level, const char *fmt, ...);

}

#endif //LOG_DAEMON_LOG_H
