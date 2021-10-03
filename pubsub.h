#ifndef LOGD_PUBSUB_H
#define LOGD_PUBSUB_H

#include "msg.h"
#include "array.h"

typedef struct {
    int fd;
    void **interests;
} subEntry;

typedef struct {
    int fd;
    int pid;
    int len: 31;
    int flags: 1;
    char tag[];
} pubEntry;

typedef struct {
    array substack;
    array publist;
    sortArray subentries;
} peerManager;

void pmInit(peerManager *o);
void pmFree(peerManager *o);
int pmNewSub(peerManager *o, int fd);
void pmFreeSub(peerManager *o, subEntry *e);
pubEntry *pmNewPub(peerManager *o, int fd, msgReqInit *req);
void pmFreePub(peerManager *o, pubEntry *e);
void pmPost(peerManager *o, const char *msg, int len);

#endif //LOGD_PUBSUB_H
