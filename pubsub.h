#ifndef LOGD_PUBSUB_H
#define LOGD_PUBSUB_H

#include "msg.h"
#include "array.h"

#define MAX_FAIL_COUNT 3
#define RING_BASEEXP2 4

typedef struct subEntry {
    int fd;
    int failCount;
    void **interests;
} subEntry;

typedef struct pubEntry {
    int fd;
    int pid;
    int len: 31;
    int flags: 1;
    char tag[];
} pubEntry;

typedef struct peerManager {
    array subStack;
    array pubList;
    sortArray subEntries;
    ringArray *ring;
} peerManager;

struct message {
    int len;
    char *data;
};

void pmInit(peerManager *o);
void pmFree(peerManager *o);
int pmNewSub(peerManager *o, int fd);
void pmFreeSub(peerManager *o, subEntry *e);
pubEntry *pmNewPub(peerManager *o, int fd, msgReqInit *req);
void pmFreePub(peerManager *o, pubEntry *e);
void pmPost(peerManager *o, const char *msg, int len);

#endif //LOGD_PUBSUB_H
