#include "pubsub.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <assert.h>

#include "macro.h"
#include "object.h"
#include "log.h"

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
    o->ringCache = newRingArray(RING_BASEEXP2);
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
    arrayIterator it;
    void *e;

    munmap(o->messageBuffer, o->ringCache->total * PAGE_SIZE);

    freeRingArray(o->ringCache);

    arrayIteratorInit(&it, &o->pubList);
    while ((e = arrayNext(&it))) free(e);
    arrayFree(&o->pubList);

    arrayIteratorInit(&it, (array *) &o->activeSubList);
    while ((e = arrayNext(&it))) free(e);
    sortArrayFree(&o->activeSubList);

    arrayIteratorInit(&it, &o->freeSubList);
    while ((e = arrayNext(&it))) free(e);
    arrayFree(&o->freeSubList);
}

int newSub(peerManager *o, int fd) {
    subEntry *e = arrayPop(&o->freeSubList);
    if (!e) {
        e = malloc(sizeof(*e));
        if (!e) return 0;
    }
    e->fd = fd;
    e->failCount = 0;
    return sortArrayPut(&o->activeSubList, e);
}

void freeSub(peerManager *o, subEntry *e) {
    close(e->fd);
    arrayPush(&o->freeSubList, e);
    sortArrayErase(&o->activeSubList, e);
}

pubEntry *newPub(peerManager *o, int fd, msgReqInit *req) {
    pubEntry *e = arrayFind(&o->pubList, &findFreePubEntry, 0);
    struct logTag *tag;

    if (!e) {
        e = malloc(sizeof(*e));
        arrayPush(&o->pubList, e);
    }
    e->fd = fd;
    e->pid = req->pid;
    e->flags = 0; /*mark inuse*/
    tag = makeObject(sizeof(*tag) + req->len, NULL, NULL);
    e->tag = tag;
    tag->len = req->len;
    memcpy(tag->data, req->tag, req->len);
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
    arrayIterator it;
    subEntry *sub;
    void *p;
    const char *msgStr;
    int msgLen;

    if (!arraySize(&o->activeSubList)) {
        log1("No subscribers, push message to RingCache.");
        if (fullRingArray(o->ringCache)) {
            p = popRingArray(o->ringCache);
            unrefMessage(p);
        } else {
            queueEntry *e;
            popQueue(&o->freeMessageQueue, e);
            p = e;
        }
        *(struct message *) p = *msg;
        pushRingArray(o->ringCache, p);
        return;
    }
    p = msg;
    while ((msg = popRingArray(o->ringCache))) {
        arrayIteratorInit(&it, (array *) &o->activeSubList);
        msgStr = msgFmt(msg, &msgLen);
        while ((sub = arrayNext(&it))) {
            if (write(sub->fd, msgStr, msgLen) != msgLen) {
                sub->failCount++;
            }
        }
        pushQueue(&o->freeMessageQueue, &msg->link);
        unrefMessage(msg);
    }
    msgStr = msgFmt(p, &msgLen);
    unrefMessage(p);
    success = fail = 0;
    arrayInit(&dead);
    arrayIteratorInit(&it, (array *) &o->activeSubList);
    while ((sub = arrayNext(&it))) {
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
    arrayIteratorInit(&it, &dead);
    while ((sub = arrayNext(&it))) {
        freeSub(o, sub);
    }
    arrayFree(&dead);
    log1("Post summarize: %d success, %d fail.", success, fail);
}
