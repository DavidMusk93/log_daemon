#include "pubsub.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

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

void pmInit(peerManager *o) {
    arrayInit(&o->subStack);
    arrayInit(&o->pubList);
    sortArrayInit(&o->subEntries, &compareSubEntry);
    o->ring = newRingArray(RING_BASEEXP2);
}

void pmFree(peerManager *o) {
    arrayIterator it;
    void *e;
    arrayIteratorInit(&it, &o->subStack);
    while ((e = arrayNext(&it))) free(e);
    arrayFree(&o->subStack);
    arrayIteratorInit(&it, &o->pubList);
    while ((e = arrayNext(&it))) free(entryToPtr(e));
    arrayFree(&o->pubList);
    arrayIteratorInit(&it, (array *) &o->subEntries);
    while ((e = arrayNext(&it))) free(e);
    sortArrayFree(&o->subEntries);
    freeRingArray(o->ring);
}

int pmNewSub(peerManager *o, int fd) {
    subEntry *e = arrayPop(&o->subStack);
    if (!e) {
        e = malloc(sizeof(*e));
        if (!e) return 0;
    }
    e->fd = fd;
    e->failCount = 0;
    return sortArrayPut(&o->subEntries, e);
}

void pmFreeSub(peerManager *o, subEntry *e) {
    close(e->fd);
    arrayPush(&o->subStack, e);
    sortArrayErase(&o->subEntries, e);
}

pubEntry *pmNewPub(peerManager *o, int fd, msgReqInit *req) {
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

void pmFreePub(peerManager *o, pubEntry *e) {
    close(e->fd);
    e->flags = 1; /*mark available*/
}

void pmPost(peerManager *o, const char *msg, int len) {
    int success, fail;
    array dead;
    arrayIterator iter;
    subEntry *sub;
    struct message *p;

    if (!arraySize(&o->subEntries)) {
        log1("No subscribers, push message to ring.");
        p = malloc(sizeof(*p) + len);
        p->len = len;
        memcpy(p->data, msg, len);
        pushRingArray(o->ring, p);
        return;
    }
    while ((p = popRingArray(o->ring))) {
        arrayIteratorInit(&iter, (array *) &o->subEntries);
        while ((sub = arrayNext(&iter))) {
            if (write(sub->fd, p->data, p->len) != p->len) {
                sub->failCount++;
            }
        }
        free(p);
    }
    success = fail = 0;
    arrayInit(&dead);
    arrayIteratorInit(&iter, (array *) &o->subEntries);
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
        pmFreeSub(o, sub);
    }
    arrayFree(&dead);
    log1("Post summarize: %d success, %d fail.", success, fail);
}
