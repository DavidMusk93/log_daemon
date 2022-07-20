#ifndef LOGD_PUBSUB_H
#define LOGD_PUBSUB_H

#include "msg.h"
#include "array.h"
#include "queue.h"

#define MAX_FAIL_COUNT 3
#define RING_BASEEXP2  4
#define PAGE_SIZE      4096

typedef struct subEntry {
    int fd;
    int failCount;
    void **interests;
} subEntry;

/* a shared object */
struct logTag {
    int len;
    char data[];
};

typedef struct pubEntry {
    int fd;
    int pid: 31;
    int flags: 1;
    struct logTag *tag;
} pubEntry;

typedef struct peerManager {
    array freeSubList;
    sortArray activeSubList;
    array pubList;
    ringArray *ringCache;
    queueEntry freeMessageQueue;
    void *messageBuffer;
} peerManager;

struct message {
    queueEntry link;
    int len;
    char data[];
};

void initPeerManager(peerManager *o);
void freePeerManager(peerManager *o);
int newSub(peerManager *o, int fd);
void freeSub(peerManager *o, subEntry *e);
pubEntry *newPub(peerManager *o, int fd, msgReqInit *req);
void freePub(peerManager *o, pubEntry *e);
void postMessage(peerManager *o, const char *msg, int len);

#endif //LOGD_PUBSUB_H
