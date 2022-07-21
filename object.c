//
// Created by Steve on 7/19/2022.
//

#include "object.h"
#include "array.h"
#include "queue.h"
#include "macro.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>

struct objectMeta {
    int refCount;
    int objectSize;
    disposeFn dispose;
    void *context;
};

#define CACHE_LINE_SIZE         64
#define ARENA_ALLOC_BLOCK_SIZE (4096*4096)
#define ARENA_SLOT_BUFFER_SIZE (sizeof(struct slot)*4096)

enum arenaFlag {
    ARENA_OBJECT_INUSE = 1 << 0,
    ARENA_BLOCK_START = 1 << 1,
    ARENA_BLOCK_END = 1 << 2,
};

struct arenaMeta {
    uint32 prevSize;
    uint32 selfFlag: 3;
    uint32 selfSize: 29;
};

struct slot {
    struct arenaMeta meta;
    queueEntry head;
};

/* a good arena means:
 *   fast find slot
 *   less overhead on similar object management
 *   less fragmentation
 *   could shrink buffer to kernel
 */
struct arena {
    sortArray listSlot;
    array listAllocPoint;
    void *victimBuffer;
    size_t victimBufferSize;
    size_t prevObjectSize;
    void *slotBuffer;
    void *slotBufferEnd;
    size_t watermark;
};

static int compareSlot(const void *l, const void *r) {
    return (int) ((struct arenaMeta *) l)->selfSize - (int) ((struct arenaMeta *) r)->selfSize;
}

#define pageAlignedAlloc(size) \
mmap(0,size,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_SHARED,-1,0)
#define cacheLineSizeAlign(size) \
(((size)+CACHE_LINE_SIZE-1)&-CACHE_LINE_SIZE)

arena *newArena() {
    arena *r = malloc(sizeof(*r));
    assert(r);

    sortArrayInit(&r->listSlot, &compareSlot);
    arrayInit(&r->listAllocPoint);

    r->victimBuffer = NULL;
    r->victimBufferSize = 0;
    r->prevObjectSize = 0;

    /* TODO: slotBuffer is only alloc once, may be a bug. */
    r->slotBuffer = pageAlignedAlloc(ARENA_SLOT_BUFFER_SIZE);
    assert(r->slotBuffer);
    r->slotBufferEnd = r->slotBuffer + ARENA_ALLOC_BLOCK_SIZE;

    r->watermark = 0;

    return r;
}

void freeArena(void *arena) {
    struct arena *o = arena;
    struct arrayIterator it;
    void *p;

    munmap(o->slotBufferEnd - ARENA_SLOT_BUFFER_SIZE, ARENA_SLOT_BUFFER_SIZE);

    arrayIteratorInit(&it, &o->listAllocPoint);
    while ((p = arrayNext(&it))) {
        munmap(p, ARENA_ALLOC_BLOCK_SIZE);
    }

    arrayFree(&o->listAllocPoint);
    sortArrayFree(&o->listSlot);
}

static void *claimSlot(struct arena *arena, struct arenaMeta *hint) {
    void **slotPointer;
    struct slot *r;
    if (sortArrayMakeSlot(&arena->listSlot, hint, &slotPointer) != 0/*new slot*/) {
        /*alloc slot*/
        r = arena->slotBuffer;
        arena->slotBuffer += sizeof(*r);
        assert(arena->slotBuffer <= arena->slotBufferEnd);
        *slotPointer = r;
        initQueue(&r->head);
    } else {
        r = *slotPointer;
    }
    return r;
}

void *claimObject(arena *arena, size_t objectSize) {
    struct arenaMeta *meta;
    struct arenaMeta hint;
    struct slot *slot;
    size_t allocSize;
    queueEntry *entry;

    allocSize = sizeof(struct arenaMeta) + objectSize;
    allocSize = cacheLineSizeAlign(allocSize);

    /* TODO: handle allocSize > (2^29-1) */
    assert(allocSize < (1 << 29));
    if (allocSize > ARENA_ALLOC_BLOCK_SIZE) {
        meta = malloc(allocSize);
        meta->selfSize = (int) (allocSize);
        return meta + 1;
    }
    hint.selfSize = (int) allocSize;
    slot = claimSlot(arena, &hint);

    if (emptyQueue(&slot->head)) {
        if (arena->victimBufferSize >= allocSize) {
            alloc:
            meta = arena->victimBuffer;
            meta->prevSize = (int) arena->prevObjectSize;
            meta->selfSize = allocSize;
            meta->selfFlag = arena->victimBufferSize == allocSize ? ARENA_BLOCK_END : 0;
            if (arena->victimBufferSize == ARENA_ALLOC_BLOCK_SIZE) {
                meta->selfFlag |= ARENA_BLOCK_START;
            }
            meta->selfFlag |= ARENA_OBJECT_INUSE;

            arena->prevObjectSize = allocSize;
            arena->victimBuffer += allocSize;
            arena->victimBufferSize -= allocSize;
            arena->watermark += allocSize;

            return meta + 1;
        }
        if (arena->victimBufferSize) {
            meta = arena->victimBuffer;
            meta->prevSize = arena->prevObjectSize;
            meta->selfSize = arena->victimBufferSize;
            meta->selfFlag = ARENA_BLOCK_END;

            hint.selfSize = meta->selfSize;
            slot = claimSlot(arena, &hint);
            entry = arena->victimBuffer + sizeof(*meta);
            pushQueue(&slot->head, entry);
        }

        arena->victimBuffer = pageAlignedAlloc(ARENA_ALLOC_BLOCK_SIZE);
        arena->victimBufferSize = ARENA_ALLOC_BLOCK_SIZE;
        arena->prevObjectSize = 0;
        arrayPush(&arena->listAllocPoint, arena->victimBuffer);
        goto alloc;
    }
    entry = nextQueueEntry(&slot->head);
    removeQueueEntry(entry);
    meta = (void *) entry - sizeof(*slot);
    meta->selfFlag |= ARENA_OBJECT_INUSE;
    return entry;
}

void reclaimObject(arena *arena, void *objectPointer) {
    struct arenaMeta *meta = objectPointer - sizeof(*meta);
    queueEntry *entry = objectPointer;
    struct slot *slot;

    if (meta->selfSize > ARENA_ALLOC_BLOCK_SIZE) {
        free(meta);
    } else {
        meta->selfFlag &= ~ARENA_OBJECT_INUSE;
        slot = claimSlot(arena, meta);
        pushQueue(&slot->head, entry);
        arena->watermark -= meta->selfSize;
        /* TODO: merge blocks */
    }
}

static arena *objectArena;

static attrCtor void initGlobalVariables() {
    objectArena = newArena();
}

static attrDtor void freeGlobalVariables() {
    freeArena(objectArena);
}

void *makeObject(size_t objectSize, disposeFn dispose, void *context) {
    struct objectMeta *meta = claimObject(objectArena, sizeof(*meta) + objectSize);
    meta->refCount = 1;
    meta->objectSize = (int) objectSize;
    meta->dispose = dispose;
    meta->context = context;
    return meta + 1;
}

void *refObject(void *objectPointer) {
    struct objectMeta *meta = objectPointer - sizeof(*meta);
    __sync_add_and_fetch(&meta->refCount, 1);
    return objectPointer;
}

int unrefObject(void *objectPointer) {
    struct objectMeta *meta = objectPointer - sizeof(*meta);
    int remainRefCount = __sync_sub_and_fetch(&meta->refCount, 1);
    if (remainRefCount == 0) {
        if (meta->dispose) {
            meta->dispose(objectPointer, meta->context);
        }
        reclaimObject(objectArena, meta);
    }
    return remainRefCount;
}

#if 0
#include <stdio.h>

_main() {
    struct arenaMeta meta = {};
    meta.selfSize = 4096;
    meta.selfFlag |= ARENA_BLOCK_START;
    log1("%x", *(uint32 *) ((void *) &meta + sizeof(meta.prevSize)));
    log1("sizeof(struct slot)=%lu", sizeof(struct slot));
    return 0;
}
#endif
