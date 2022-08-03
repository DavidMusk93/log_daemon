#include "array.h"

#include <stdlib.h>
#include <string.h>

#define MIN_BASEEXP2 3
#define MAX_BASEEXP2 15

ring *newRing(uint32 baseExp2) {
    uint32 itemCount;
    if (baseExp2 < MIN_BASEEXP2)
        baseExp2 = MIN_BASEEXP2;
    if (baseExp2 > MAX_BASEEXP2)
        baseExp2 = MAX_BASEEXP2;
    itemCount = 1 << baseExp2;
    ring *r = malloc(sizeof(*r) + sizeof(void *) * itemCount);
    r->total = itemCount;
    r->mask = itemCount - 1;
    r->indexProduce = 0;
    r->indexConsume = 0;
    r->data = (void **) (r + 1);
    return r;
}

void freeRing(ring *ring) {
    free(ring);
}

int pushRing(ring *ring, void *item) {
    ring->data[ring->indexProduce++ & ring->mask] = item;
    return 0;
}

void *popRing(ring *ring) {
    if (ring->indexConsume == ring->indexProduce) {
        return NULL;
    }
    /* TODO: fix indexConsume<(uint32)-1, indexProduce>0 */
    if (ring->indexConsume + ring->total <= ring->indexProduce ||
        ring->indexConsume > ring->indexProduce/*has reset*/) {
        ring->indexConsume = ring->indexProduce - ring->total;
    }
    return ring->data[ring->indexConsume++ & ring->mask];
}

int fullRing(ring *ring) {
    return ring->indexConsume + ring->total == ring->indexProduce; /* loose condition */
}

void arrayInit(array *o) {
    o->a = o->static_items;
    o->i = 0;
    o->n = ARRAY_NUM_STATIC_ITEMS;
}

void arrayFree(array *o) {
    if (o->a != o->static_items) free(o->a);
}

int arrayPush(array *o, void *e) {
    if (o->i == o->n) {
        o->a = realloc(o->a == o->static_items ? 0 : o->a, o->n * 2 * sizeof(void *));
        if (o->n == ARRAY_NUM_STATIC_ITEMS) {
            memcpy(o->a, o->static_items, sizeof(void *) * ARRAY_NUM_STATIC_ITEMS);
        }
        o->n *= 2;
        if (!o->a) return 0;
    }
    o->a[o->i++] = e;
    return 1;
}

void *arrayPop(array *o) {
    if (!o->i) return 0;
    return o->a[--o->i];
}

void *arrayTop(array *o) {
    if (!o->i) return 0;
    return o->a[o->i - 1];
}

void *arrayFind(array *o, int (*match)(const void *, void *), void *data) {
    unsigned i;
    for (i = 0; i < o->i; i++) {
        if (match(o->a[i], data)) {
            return o->a[i];
        }
    }
    return 0;
}

void sortArrayInit(sortArray *o, compare cmp) {
    arrayInit((array *) o);
    o->cmp = cmp;
}

void sortArrayFree(sortArray *o) {
    arrayFree((array *) o);
}

static int sortArrayFind(sortArray *o, void *e, unsigned *posptr) {
    int rc;
    if (posptr) *posptr = 0;
    if (!o->i) return 0;
    if ((rc = o->cmp(e, o->a[0])) <= 0) return rc == 0;
    if ((rc = o->cmp(e, o->a[o->i - 1])) >= 0) {
        if (posptr) *posptr = o->i - (rc == 0);
        return rc == 0;
    }
    unsigned i, j, m; /* (i,j) */
    i = 1;
    j = o->i - 2;
    do {
        if (i > j) break;
        m = i + (j - i) / 2;
        rc = o->cmp(e, o->a[m]);
        if (rc == 0) {
            if (posptr) *posptr = m;
            return 1; /* gotcha */
        } else if (rc > 0) {
            i = m + 1;
        } else {
            j = m - 1;
        }
    } while (1);
    if (posptr) *posptr = i;
    return 0;
}

int sortArrayPut(sortArray *o, void *e) {
    if (!e) return 0;
    unsigned pos;
    if (sortArrayFind(o, e, &pos)) return 0; /* already exist */
    arrayPush((array *) o, 0);
    unsigned i = o->i - 1;
    for (; i > pos; i--) {
        o->a[i] = o->a[i - 1];
    }
    o->a[pos] = e;
    return 1;
}

void *sortArrayErase(sortArray *o, void *e) {
    unsigned pos;
    if (!sortArrayFind(o, e, &pos)) return 0;
    e = o->a[pos];
    for (; pos + 1 < o->i; pos++) {
        o->a[pos] = o->a[pos + 1];
    }
    --o->i;
    return e;
}

int sortArrayMakeSlot(sortArray *o, void *hint, void ***linkSlot) {
    unsigned pos;
    if (sortArrayFind(o, hint, &pos)) {
        *linkSlot = &o->a[pos];
        return 0;
    }
    arrayPush((array *) o, 0); /* maybe realloc */
    *linkSlot = &o->a[pos];
    unsigned i = o->i - 1;
    for (; i > pos; i--) {
        o->a[i] = o->a[i - 1];
    }
    return 1;
}

void initArrayIterator(arrayIterator *o, array *a) {
    o->array = a;
    o->i = 0;
}

void *nextArrayElement(arrayIterator *o) {
    if (o->i < o->array->i) {
        return o->array->a[o->i++];
    }
    return 0;
}

#ifdef ARRAY_TEST
#include <stdio.h>
#include <time.h>

#include "macro.h"

int intcmp(const void *l, const void *r) {
    return (int) (long) l - (int) (long) r;
}

_main() {
#ifndef ARRAYLEN
# define ARRAYLEN 1024
#endif
    sortArray sa;
    sortArrayInit(&sa, &intcmp);
    srand(time(0));
    int i;
    for (i = 0; i < ARRAYLEN; i++) {
        sortArrayPut(&sa, (void *) (long) rand());
    }
    arrayIterator it;
    arrayIteratorInit(&it, (array *) &sa);
    void *e;
    i = 0;
    while ((e = arrayNext(&it))) {
        printf("#%08d %#010x\n", ++i, (int) (long) e);
    }
    sortArrayFree(&sa);
    return 0;
}
#endif
