#include "pubsub.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/socket.h>

#include "macro.h"
#include "object.h"
#include "log.h"
#include "filter.h"

static int compareSubEntry(const void *l, const void *r) {
    return *(int *) l - *(int *) r;
}

static int findFreePubEntry(const void *e, void *data) {
    (void) data;
    return ((pubEntry *) e)->flags;
}

void initPeerManager(peerManager *o) {
    arrayInit(&o->freeSubList);
    sortArrayInit(&o->activeSubList, &compareSubEntry);
    arrayInit(&o->pubList);
    o->ringCache = newRing(RING_BASEEXP2);
    initQueue(&o->freeMessageQueue);

    size_t bufSize = o->ringCache->total * sizeof(struct message);
    bufSize = pageSizeAlign(bufSize);
    struct message *msg;
    void *p, *e;
    o->messageBuffer = mmap(0, bufSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    assert(o->messageBuffer != MAP_FAILED);
    for (p = o->messageBuffer, e = p + bufSize; p < e; p += sizeof(*msg)) {
        msg = p;
        pushQueue(&o->freeMessageQueue, &msg->link);
    }
}

void freePeerManager(peerManager *o) {
    iteratorArray it;
    void *e;

    /* TODO: trace memory map & unmap */
    munmap(o->messageBuffer, pageSizeAlign(o->ringCache->total * sizeof(struct message)));

    freeRing(o->ringCache);

    initIteratorArray(&it, &o->pubList);
    while ((e = nextElementArray(&it))) free(e);
    arrayFree(&o->pubList);

    initIteratorArray(&it, (array *) &o->activeSubList);
    while ((e = nextElementArray(&it))) free(e);
    sortArrayFree(&o->activeSubList);

    initIteratorArray(&it, &o->freeSubList);
    while ((e = nextElementArray(&it))) free(e);
    arrayFree(&o->freeSubList);
}

int newSub(peerManager *o, int fd, initLogRequest *req) {
    subEntry *e = arrayPop(&o->freeSubList);
    if (!e) {
        e = malloc(sizeof(*e));
        if (!e) return 0;
    }
    e->fd = fd;
    e->failCount = 0;

    byteReader r = {req->tag, req->tag + req->len};
    unpackFilter(&r, &e->f);

    return sortArrayPut(&o->activeSubList, e);
}

void freeSub(peerManager *o, subEntry *e) {
    close(e->fd);
    if (e->f) {
        freeFilter(e->f);
    }

    arrayPush(&o->freeSubList, e);
    sortArrayErase(&o->activeSubList, e);
}

pubEntry *newPub(peerManager *o, int fd, initLogRequest *req) {
    pubEntry *e = arrayFind(&o->pubList, &findFreePubEntry, 0);
    if (!e) {
        e = malloc(sizeof(*e));
        arrayPush(&o->pubList, e);
    }

    e->fd = fd;
    e->pid = req->pid;
    e->flags = 0; /*mark inuse*/

    refVarchar(e->tag, req->tag, req->len);
    return e;
}

void freePub(peerManager *o, pubEntry *e) {
    opList((void), o);
    close(e->fd);
    e->flags = 1; /*mark available*/
    unrefObject(e->tag);
}

#define unrefMessage(p) \
unrefObject(((struct message*)(p))->tag);\
unrefObject(((struct message*)(p))->content)
#define MSG_FMT "%u.%06u %d#%d %.*s %s %.*s"

static __thread char messageBuffer[PAGE_SIZE];

static const char *levelStr(int level) {
#define __case(x) case LOG_LEVEL_##x:return #x
    switch (level) {
        __case(DEBUG);
        __case(INFO);
        __case(WARN);
        __case(ERROR);
        default:
            return "UNKNOWN";
    }
#undef __case
}

static const char *msgFmt(struct message *msg, int *ptrLen) {
    const char *fmt = msg->content->data[msg->content->len - 1] == '\n' ? MSG_FMT : MSG_FMT "\n";
    *ptrLen = sprintf(messageBuffer, fmt, msg->sec, msg->us, msg->pid, msg->tid, msg->tag->len, msg->tag->data,
                      levelStr(msg->level), msg->content->len, msg->content->data);
    return messageBuffer;
}

void postMessage(peerManager *o, struct message *msg) {
    int success, fail;
    array dead;
    iteratorArray it;
    subEntry *sub;
    void *p;
    const char *msgStr;
    int msgLen;
    int state;
    socklen_t stateLen = sizeof(state);
    queueEntry *e;

    if (!arraySize(&o->activeSubList)) {
        cache:
        log1("No subscribers, push message to RingCache.");
        if (isFullRing(o->ringCache)) {
            p = popRing(o->ringCache);
            unrefMessage(p);
        } else {
            popQueue(&o->freeMessageQueue, e);
            p = e;
        }
        *(struct message *) p = *msg;
        pushRing(o->ringCache, p);
        return;
    }
    p = msg;
    while ((msg = popRing(o->ringCache))) {
        initIteratorArray(&it, (array *) &o->activeSubList);
        msgStr = msgFmt(msg, &msgLen);
        while ((sub = nextElementArray(&it))) {
            if ((!sub->f/*all interest*/ || evalFilter(sub->f, msg)/*hit*/) &&
                write(sub->fd, msgStr, msgLen) != msgLen) {
                sub->failCount++;
            }
        }
        pushQueue(&o->freeMessageQueue, &msg->link);
        unrefMessage(msg);
    }
    msgStr = msgFmt(p, &msgLen);
    success = fail = 0;
    arrayInit(&dead);
    initIteratorArray(&it, (array *) &o->activeSubList);
    while ((sub = nextElementArray(&it))) {
        getsockopt(sub->fd, SOL_SOCKET, SO_ERROR, &state, &stateLen);
        if (state) { /*sock inactive*/
            arrayPush(&dead, sub);
            continue;
        }
        if (sub->f && !evalFilter(sub->f, p)) { /*not interest*/
            continue;
        }
        if (write(sub->fd, msgStr, msgLen) == msgLen) {
            success++;
            sub->failCount = 0;
        } else {
            fail++;
            if (++sub->failCount > MAX_FAIL_COUNT) {
                arrayPush(&dead, sub);
            }
        }
    }

    if (success == 0 && fail == 0) {
        msg = p;
        goto cache;
    }

    unrefMessage(p);
    initIteratorArray(&it, &dead);
    while ((sub = nextElementArray(&it))) {
        freeSub(o, sub);
    }
    arrayFree(&dead);
    log1("Post summarize: %d success, %d fail.", success, fail);
}
