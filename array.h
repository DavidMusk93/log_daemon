#ifndef LOGDAEMON_ARRAY_H
#define LOGDAEMON_ARRAY_H

#define ARRAY_NUM_STATIC_ITEMS 32

typedef int (*compare)(const void *, const void *);
typedef unsigned uint32;

#define ARRAY_ELEMENTS(a, i, n) \
void **a;\
unsigned i,n;\
void *static_items[ARRAY_NUM_STATIC_ITEMS]

typedef struct array {
    ARRAY_ELEMENTS(a, i, n);
} array;

typedef struct sortArray {
    ARRAY_ELEMENTS(a, i, n);
    compare cmp;
} sortArray;

typedef struct arrayIterator {
    array *array;
    unsigned i;
} arrayIterator;

typedef struct ringArray {
    uint32 total;
    uint32 mask;
    uint32 indexProduce;
    uint32 indexConsume;
    void *data[];
} ringArray;

ringArray *newRingArray(uint32 baseExp2);
void freeRingArray(ringArray *ring);
int pushRingArray(ringArray *ring, void *item);
void *popRingArray(ringArray *ring);

void arrayInit(array *o);
void arrayFree(array *o);
int arrayPush(array *o, void *e);
void *arrayPop(array *o);
void *arrayTop(array *o);
void *arrayFind(array *o, int (*match)(const void *, void *), void *data);
#define arraySize(o) (o)->i

void sortArrayInit(sortArray *o, compare cmp);
void sortArrayFree(sortArray *o);
int sortArrayPut(sortArray *o, void *e);
void *sortArrayErase(sortArray *o, void *e);

void arrayIteratorInit(arrayIterator *o, array *a);
void *arrayNext(arrayIterator *o);

#endif //LOGDAEMON_ARRAY_H
