#include "pubsub.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <assert.h>

#include "macro.h"

#define allocPrefixLen sizeof(long)
#define ptrToEntry(p) ((char*)(p)+allocPrefixLen)
#define entryToPtr(e) ((char*)(e)-allocPrefixLen)
#define prefixFromPtr(p) ((long*)(p))[0]
#define prefixFromEntry(e) ((long*)(e))[-1]

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

    size_t bufSize = o->ringCache->total * PAGE_SIZE;
    struct message *msg;
    void *p, *e;
    o->messageBuffer = mmap(0, bufSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    assert(o->messageBuffer != MAP_FAILED);
    for (p = o->messageBuffer, e = p + bufSize; p < e; p += PAGE_SIZE) {
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
    while ((e = arrayNext(&it))) free(entryToPtr(e));
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
    if (!e || prefixFromEntry(e) < req->len) {
        void *t = realloc(e ? entryToPtr(e) : 0, allocPrefixLen + sizeof(*e) + req->len);
        if (!t) return 0;
        prefixFromPtr(t) = req->len;
        t = ptrToEntry(t);
        if (!e) arrayPush(&o->pubList, t);
        e = t;
    }
    e->fd = fd;
    e->pid = req->pid;
    e->len = req->len;
    e->flags = 0; /*mark inuse*/
    memcpy(e->tag, req->tag, e->len);
    return e;
}

void freePub(peerManager *o, pubEntry *e) {
    close(e->fd);
    e->flags = 1; /*mark available*/
}

void postMessage(peerManager *o, const char *msg, int len) {
    int success, fail;
    array dead;
    arrayIterator iter;
    subEntry *sub;
    struct message *p;

    if (!arraySize(&o->activeSubList)) {
        log1("No subscribers, push message to RingCache.");
        if (fullRingArray(o->ringCache)) {
            p = popRingArray(o->ringCache);
        } else {
            queueEntry *e;
            popQueue(&o->freeMessageQueue, e);
            p = (struct message *) e;
        }
        p->len = len;
        memcpy(p->data, msg, len);
        pushRingArray(o->ringCache, p);
        return;
    }
    while ((p = popRingArray(o->ringCache))) {
        arrayIteratorInit(&iter, (array *) &o->activeSubList);
        while ((sub = arrayNext(&iter))) {
            if (write(sub->fd, p->data, p->len) != p->len) {
                sub->failCount++;
            }
        }
        pushQueue(&o->freeMessageQueue, &p->link);
    }
    success = fail = 0;
    arrayInit(&dead);
    arrayIteratorInit(&iter, (array *) &o->activeSubList);
    while ((sub = arrayNext(&iter))) {
        if (write(sub->fd, msg, len) == len) {
            success++;
            sub->failCount = 0;
        } else {
            fail++;
            if (++sub->failCount > MAX_FAIL_COUNT) {
                arrayPush(&dead, sub);
            }
        }
    }
    arrayIteratorInit(&iter, &dead);
    while ((sub = arrayNext(&iter))) {
        freeSub(o, sub);
    }
    arrayFree(&dead);
    log1("Post summarize: %d success, %d fail.", success, fail);
}
