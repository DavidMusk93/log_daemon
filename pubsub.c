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
    arrayInit(&o->substack);
    arrayInit(&o->publist);
    sortArrayInit(&o->subentries, &compareSubEntry);
}

void pmFree(peerManager *o) {
    arrayIterator it;
    void *e;
    arrayIteratorInit(&it, &o->substack);
    while ((e = arrayNext(&it))) free(e);
    arrayFree(&o->substack);
    arrayIteratorInit(&it, &o->publist);
    while ((e = arrayNext(&it))) free(entryToPtr(e));
    arrayFree(&o->publist);
    arrayIteratorInit(&it, (array *) &o->subentries);
    while ((e = arrayNext(&it))) free(e);
    sortArrayFree(&o->subentries);
}

int pmNewSub(peerManager *o, int fd) {
    subEntry *e = arrayPop(&o->substack);
    if (!e) {
        e = malloc(sizeof(*e));
        if (!e) return 0;
    }
    e->fd = fd;
    return sortArrayPut(&o->subentries, e);
}

void pmFreeSub(peerManager *o, subEntry *e) {
    close(e->fd);
    arrayPush(&o->substack, e);
    sortArrayErase(&o->subentries, e);
}

pubEntry *pmNewPub(peerManager *o, int fd, msgReqInit *req) {
    pubEntry *e = arrayFind(&o->publist, &findFreePubEntry, 0);
    if (!e || prefixFromEntry(e) < req->len) {
        void *t = realloc(e ? entryToPtr(e) : 0, allocPrefixLen + sizeof(*e) + req->len);
        if (!t) return 0;
        prefixFromPtr(t) = req->len;
        t = ptrToEntry(t);
        if (!e) arrayPush(&o->publist, t);
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
    arrayIterator it;
    void *e;
    if (!arraySize(&o->subentries)) {
        log1("No subscribers, drop message.");
        return;
    }
    success = fail = 0;
    arrayInit(&dead);
    arrayIteratorInit(&it, (array *) &o->subentries);
    while ((e = arrayNext(&it))) {
        if (write(((subEntry *) e)->fd, msg, len) == len) success++;
        else {
            fail++;
            arrayPush(&dead, e);
        }
    }
    arrayIteratorInit(&it, &dead);
    while ((e = arrayNext(&it))) {
        pmFreeSub(o, e);
    }
    arrayFree(&dead);
    log1("Post summarize: %d success, %d fail.", success, fail);
}
