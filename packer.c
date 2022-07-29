//
// Created by Steve on 7/28/2022.
//

#include "packer.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "filter.h"
#include "msg.h"

struct byteWriter {
    int i;
    int n;
    char a[];
};

#define WRITER_CAPACITY_EXPAND_THRESHOLD 256

byteWriter *newByteWriter(int capacity) {
    if (capacity < WRITER_CAPACITY_EXPAND_THRESHOLD) {
        capacity = WRITER_CAPACITY_EXPAND_THRESHOLD;
    }

    byteWriter *r = malloc(sizeof(*r) + capacity);
    assert(r);

    r->i = 0;
    r->n = capacity;
    return r;
}

void freeByteWriter(void *ptrPtr) {
    free(*(void **) ptrPtr);
}

byteWriter *putData(struct byteWriter *w, const void *buf, int bufLen, int *linkOff) {
    if (w->i + bufLen > w->n) {
        w->n = w->i + bufLen + WRITER_CAPACITY_EXPAND_THRESHOLD;
        w = realloc(w, w->n);
        assert(w);
    }

    memcpy(w->a + w->i, buf, bufLen);

    if (linkOff) {
        *linkOff = w->i;
    }

    w->i += bufLen;

    return w;
}

void *dataPtr(struct byteWriter *w, int *ptrLen) {
    if (ptrLen) {
        *ptrLen = w->i;
    }
    return w->a;
}

int getData(byteReader *r, void *buf, int bufLen) {
    if (r->s + bufLen <= r->e) {
        if (buf) {
            memcpy(buf, r->s, bufLen);
        }
        r->s += bufLen;
        return bufLen;
    }
    return 0;
}

void initPackerFilter(struct packerFilter *o) {
    o->filterCount = 0;
    o->w = newByteWriter(0);
    struct initLogRequest req = {};
    o->w = putData(o->w, &req, sizeof(req), NULL);
    o->w = putData(o->w, &o->filterCount, sizeof(o->filterCount), &o->indexCount);
}

void *finalizePackerFilter(struct packerFilter *o, int *ptrLen) {
    void *p = dataPtr(o->w, ptrLen);
    if (o->filterCount) {
        memcpy(p + o->indexCount, &o->filterCount, sizeof(o->filterCount));
    } else {
        *ptrLen -= sizeof(o->filterCount);
    }
    return p;
}

void freePackerFilter(void *ptr) {
    struct packerFilter *o = ptr;
    freeByteWriter(&o->w);
}

int packPid(struct packerFilter *o, int pid) {
    int type = FILTER_PID;
    o->w = putData(o->w, &type, sizeof(type), NULL);
    o->w = putData(o->w, &pid, sizeof(pid), NULL);
    o->filterCount++;
    return 0;
}

int packLevel(struct packerFilter *o, int level) {
    int type = FILTER_LEVEL;
    o->w = putData(o->w, &type, sizeof(type), NULL);
    o->w = putData(o->w, &level, sizeof(level), NULL);
    o->filterCount++;
    return 0;
}

int packTag(struct packerFilter *o, const char *tag, int tagLen) {
    int type = FILTER_TAG;
    o->w = putData(o->w, &type, sizeof(type), NULL);
    o->w = putData(o->w, &tagLen, sizeof(tagLen), NULL);
    o->w = putData(o->w, tag, tagLen, NULL);
    o->filterCount++;
    return 0;
}

int unpackFilter(byteReader *r, struct filter **ptrFilter) {
    int t, filterCount = 0;
    const void *tag;
    int i = 0;
    struct filter *f = NULL, *f2;
#define getData(...) do{ \
    if(getData(__VA_ARGS__)==0){\
        goto end;\
    }\
}while(0)
    getData(r, &filterCount, sizeof(filterCount));
    for (; i < filterCount; i++) {
        getData(r, &t, sizeof(t));
        if (t == FILTER_PID) {
            getData(r, &t, sizeof(t));
            f2 = newPidFilter(t);
        } else if (t == FILTER_LEVEL) {
            getData(r, &t, sizeof(t));
            f2 = newLevelFilter(t);
        } else if (t == FILTER_TAG) {
            getData(r, &t, sizeof(t));
            tag = r->s;
            getData(r, NULL, t);
            f2 = newTagFilter(tag, t);
        } else {
            /* unknown type */
            break;
        }
        f = f ? filterOr(f, f2) : f2;
    }
    end:
    *ptrFilter = f;
    return i;
#undef getData
}
