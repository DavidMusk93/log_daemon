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

#define _postLogDetailed(level, fmt, ...) logPost(level,"%s:%d " fmt "\n",__FILE__,__LINE__,##__VA_ARGS__)
#define logInfo(...) _postLogDetailed(LOG_LEVEL_INFO,__VA_ARGS__)

#endif //LOG_DAEMON_LOG_H
