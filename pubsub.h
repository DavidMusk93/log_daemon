#ifndef LOGD_PUBSUB_H
#define LOGD_PUBSUB_H

#include "msg.h"
#include "array.h"
#include "queue.h"
#include "packer.h"

#define RING_BASEEXP2_THRESHOLD_LOW  4
#define RING_BASEEXP2_THRESHOLD_HIGH 16
#define MAX_FAIL_COUNT 3
#ifndef RING_BASEEXP2
# define RING_BASEEXP2  8
#endif

#if RING_BASEEXP2 > RING_BASEEXP2_THRESHOLD_HIGH
# undef RING_BASEEXP2
# define RING_BASEEXP2 RING_BASEEXP2_THRESHOLD_HIGH
#endif

#if RING_BASEEXP2 < RING_BASEEXP2_THRESHOLD_LOW
# undef RING_BASEEXP2
# define RING_BASEEXP2 RING_BASEEXP2_THRESHOLD_LOW
#endif

#define PAGE_SIZE        4096
#define pageSizeAlign(x) ((x+PAGE_SIZE-1)&-PAGE_SIZE)

typedef struct subEntry {
    int fd;
    int failCount;
    struct filter *f;
} subEntry;

/* a shared object */
typedef struct varchar {
    int len;
    char data[];
} varchar;
#define refVarchar(_ptr, _data, _datelen) \
(_ptr)=makeObject(sizeof(*(_ptr))+(_datelen),NULL,NULL);\
(_ptr)->len=_datelen;\
memcpy((_ptr)->data,_data,_datelen)

typedef struct pubEntry {
    int fd;
    int pid: 31;
    int flags: 1;
    varchar *tag;
} pubEntry;

typedef struct peerManager {
    array freeSubList;
    sortArray activeSubList;
    array pubList;
    ring *ringCache;
    queueEntry freeMessageQueue;
    void *messageBuffer;
} peerManager;

struct message {
    queueEntry link;
    uint32 sec;
    uint32 us;
    int pid;
    int tid: 29;
    int level: 3;
    varchar *tag;
    varchar *content;
};

void initPeerManager(peerManager *o);
void freePeerManager(peerManager *o);
int newSub(peerManager *o, int fd, initLogRequest *req);
void freeSub(peerManager *o, subEntry *e);
pubEntry *newPub(peerManager *o, int fd, initLogRequest *req);
void freePub(peerManager *o, pubEntry *e);
void postMessage(peerManager *o, struct message *msg);

#endif //LOGD_PUBSUB_H
