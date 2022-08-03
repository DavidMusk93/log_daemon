//
// Created by Steve on 7/26/2022.
//

#include "filter.h"

#include <string.h>

#include "pubsub.h"
#include "macro.h"
#include "object.h"

static filterResult matchPid(filter *f, struct message *msg) {
    return f->pid == msg->pid ? FILTER_RESULT_TRUE : FILTER_RESULT_FALSE;
}

static filterResult matchLevel(filter *f, struct message *msg) {
    return f->level <= msg->level ? FILTER_RESULT_TRUE : FILTER_RESULT_FALSE;
}

static filterResult matchTag(filter *f, struct message *msg) {
    if (f->tag == msg->tag) { /*same object*/
        return FILTER_RESULT_TRUE;
    }
    if (f->tag->len == msg->tag->len && memcmp(f->tag->data, msg->tag->data, f->tag->len) == 0) {
        unrefObject(f->tag);
        f->tag = refObject(msg->tag);
        return FILTER_RESULT_TRUE;
    }
    return FILTER_RESULT_FALSE;
}

static void dispose(void *object, void *context) {
    opList((void), context);
    filter *f = object;
    if (f->type == FILTER_TAG) {
        unrefObject(f->tag);
    }
    if (f->opType == FILTER_OP_AND || f->opType == FILTER_OP_OR) {
        unrefObject(f->f2); /*chain dispose*/
    }
}

filter *newPidFilter(int pid) {
    filter *r = makeObject(sizeof(*r), &dispose, NULL);
    r->type = FILTER_PID;
    r->opType = FILTER_OP_NONE;
    r->pid = pid;
    r->fn = &matchPid;
    r->f2 = NULL;
    return r;
}

filter *newLevelFilter(int level) {
    filter *r = makeObject(sizeof(*r), &dispose, NULL);
    r->type = FILTER_LEVEL;
    r->opType = FILTER_OP_NONE;
    r->level = level;
    r->fn = &matchLevel;
    r->f2 = NULL;
    return r;
}

filter *newTagFilter(const char *tag, int tagLen) {
    filter *r = makeObject(sizeof(*r), &dispose, NULL);
    r->type = FILTER_TAG;
    r->opType = FILTER_OP_NONE;

    refVarchar(r->tag, tag, tagLen);

    r->fn = &matchTag;
    r->f2 = NULL;
    return r;
}

void freeFilter(filter *f) {
    unrefObject(f);
}

filterResult evalFilter(filter *f, struct message *msg) {
    int r = f->fn(f, msg);
    if (f->opType == FILTER_OP_NOT) {
        return !r;
    }
    if (f->opType == FILTER_OP_AND) {
        return r && evalFilter(f->f2, msg);
    }
    if (f->opType == FILTER_OP_OR) {
        return r /*lazy*/|| evalFilter(f->f2, msg);
    }
    return r;
}

filter *filterNot(filter *f) {
    f->opType = FILTER_OP_NOT;
    return f;
}

filter *filterAnd(filter *f, filter *f2) {
    f->opType = FILTER_OP_AND;
    f->f2 = f2;
    return f;
}

filter *filterOr(filter *f, filter *f2) {
    f->opType = FILTER_OP_OR;
    f->f2 = f2;
    return f;
}

#ifdef TEST_FILTER

#include "log.h"
#include <stdio.h>
#include <assert.h>

_main() {
    struct message msg = {
            .pid=1234,
            .level=LOG_LEVEL_INFO,
    };
    char tag[] = "filter_test";
    int taglen = (int) sizeof(tag) - 1;

    refVarchar(msg.tag, tag, taglen);

    filter *f = newPidFilter(1324);
    filterOr(f, newTagFilter(tag, taglen));

    filterResult r = evalFilter(f, &msg);

    int printCount = 0;
    int x = 1 || (printCount = log1("eager"), 2);
    assert(x && printCount == 0);

    unrefObject(msg.tag);
    freeFilter(f);
    return r == FILTER_RESULT_TRUE ? 0 : 1;
}

#endif
