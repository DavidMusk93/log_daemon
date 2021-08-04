#ifndef LOG_DAEMON_MSG_H
#define LOG_DAEMON_MSG_H

enum {
    LOGPEERPUB,
    LOGPEERSUB,
};

struct MsgStr {
    int len;
};

struct MsgReqHello : MsgStr {
    int type;
    int pid;
//    char *tag;
#define MSGREQSTR(p) ((char*)(p)+sizeof(MsgReqHello))
};

struct MsgResHello {
    int status;
};

struct MsgLog : MsgStr {
    int sec;
    int us;
    int tid;
    int level;
//    char *msg;
#define MSGLOGSTR(p) ((char*)(p)+sizeof(MsgLog))
};

#endif //LOG_DAEMON_MSG_H
