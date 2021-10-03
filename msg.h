#pragma once

enum {
    LOG_ROLE_PUB,
    LOG_ROLE_SUB,
};

typedef struct {
    int len;
    int pid: 28;
    int role: 4;
    char tag[];
} msgReqInit;

typedef struct {
    int status;
} msgResInit;

typedef struct {
    int len;
    unsigned sec;
    unsigned us;
    int tid: 28;
    int level: 4;
    char data[];
} msgLog;
