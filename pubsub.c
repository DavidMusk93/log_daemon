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
    return ((subEntry *) l)->fd - ((subEntry *) r)->fd;
}

static int availablePubEntry(const void *e, void *data) {
    opList((void), data);
    return ((pubEntry *) e)->flags;
}

void initPeerManager(peerManager *o) {
    arrayInit(&o->subFreeList);
    sortArrayInit(&o->subActiveList, &compareSubEntry);
    initQueue(&o->subPendingList);

    arrayInit(&o->pubList);
    o->cache = newRing(RING_BASEEXP2);
    initQueue(&o->freeMessageQueue);

    size_t bufSize = o->cache->total * sizeof(struct message);
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

#define unrefMessage(p) \
unrefObject(((struct message*)(p))->tag);\
unrefObject(((struct message*)(p))->content)

void freePeerManager(peerManager *o) {
    arrayIterator it;
    void *e;

    /* unref message (tag & content) before munmap its storage */
    while ((e = popRing(o->cache))) {
        unrefMessage(e);
    }

    /* TODO: trace memory map & unmap */
    munmap(o->messageBuffer, pageSizeAlign(o->cache->total * sizeof(struct message)));
    freeRing(o->cache);

    initArrayIterator(&it, &o->pubList);
    while ((e = nextArrayElement(&it))) free(e);
    arrayFree(&o->pubList);

    foreachQueue(&o->subPendingList, entry) {
        arrayPush(&o->subFreeList, entry); /* delegate reclaim */
    }

    initArrayIterator(&it, (array *) &o->subActiveList);
    while ((e = nextArrayElement(&it))) free(e);
    sortArrayFree(&o->subActiveList);

    initArrayIterator(&it, &o->subFreeList);
    while ((e = nextArrayElement(&it))) free(e);
    arrayFree(&o->subFreeList);
}

int newSub(peerManager *o, int fd, initLogRequest *req) {
    subEntry *e = arrayPop(&o->subFreeList);
    if (!e) {
        e = malloc(sizeof(*e));
        if (!e) return 0;
    }
    e->fd = fd;
    e->failCount = 0;
    e->f = NULL;

    if (req->len) {
        byteReader r = {req->tag, req->tag + req->len};
        unpackFilter(&r, &e->f);
    }

    pushQueue(&o->subPendingList, &e->link);
    return 1;
}

void freeSub(peerManager *o, subEntry *e) {
    close(e->fd);
    if (e->f) {
        freeFilter(e->f);
    }

    arrayPush(&o->subFreeList, e);
    sortArrayErase(&o->subActiveList, e);
}

pubEntry *newPub(peerManager *o, int fd, initLogRequest *req) {
    pubEntry *e = arrayFind(&o->pubList, &availablePubEntry, 0);
    if (!e) {
        e = malloc(sizeof(*e));
        arrayPush(&o->pubList, e);
    }

    e->fd = fd;
    e->pid = req->pid;
    e->flags = 0; /* mark inuse */

    refVarchar(e->tag, req->tag, req->len);
    return e;
}

void freePub(peerManager *o, pubEntry *e) {
    opList((void), o);
    close(e->fd);
    e->flags = 1; /* mark available */
    unrefObject(e->tag);
}

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
    const char *msgstr;
    int msglen;
    int state;
    socklen_t statelen = sizeof(state);
    queueEntry *e;

    /* post cached message to pending subscribers */
    if (!emptyQueue(&o->subPendingList)) {
        struct ring cacheview = *o->cache; /* iterator alike */
        while ((p = popRing(&cacheview))) {
            msgstr = msgFmt(p, &msglen);
            foreachQueue(&o->subPendingList, entry) {
                sub = (void *) entry;
                if (!sub->f/*all interest*/|| evalFilter(sub->f, msg)/*hit*/) {
                    write(sub->fd, msgstr, msglen);
                }
            }
        }

        /* pending to active */
        for (;;) {
            popQueue(&o->subPendingList, e);
            sortArrayPut(&o->subActiveList, e);
            if (emptyQueue(&o->subPendingList)) {
                break;
            }
        }
    }

    /* cache message first */
    if (fullRing(o->cache)) {
        p = popRing(o->cache);
        unrefMessage(p);
    } else {
        popQueue(&o->freeMessageQueue, e);
        p = e;
    }
    *(struct message *) p = *msg;
    pushRing(o->cache, p);

    if (arraySize(&o->subActiveList) == 0) {
        log1("no subscriber, only cache message");
        return;
    }

    /* post this fresh message to all active subscribers */
    msgstr = msgFmt(msg, &msglen);
    success = fail = 0;

    arrayInit(&dead);
    initArrayIterator(&it, (array *) &o->subActiveList);

    while ((sub = nextArrayElement(&it))) {
        getsockopt(sub->fd, SOL_SOCKET, SO_ERROR, &state, &statelen);
        if (state) { /* sock inactive */
            arrayPush(&dead, sub);
            continue;
        }
        if (sub->f && !evalFilter(sub->f, p)) { /*not interest*/
            continue;
        }
        if (write(sub->fd, msgstr, msglen) == msglen) {
            success++;
            sub->failCount = 0;
        } else {
            fail++;
            if (++sub->failCount > MAX_FAIL_COUNT) {
                arrayPush(&dead, sub);
            }
        }
    }

    initArrayIterator(&it, &dead);
    while ((sub = nextArrayElement(&it))) {
        freeSub(o, sub);
    }
    arrayFree(&dead);
    log1("Post summarize: %d success, %d fail.", success, fail);
}
