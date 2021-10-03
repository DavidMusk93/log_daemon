#ifndef LOG_DAEMON_LOG_H
#define LOG_DAEMON_LOG_H

#define LOGIPC "/tmp/log.ipc"
#define LOGBUFLEN 2048
#define LOGMSGLEN 4096

#define LOGMSGRECVTO 500 /*milliseconds*/
#define LOGRECVMAXRETRY 3

enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
};

int logInit(const char *tag);
int logPost(int level, const char *fmt, ...);

#endif //LOG_DAEMON_LOG_H
