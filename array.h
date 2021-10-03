#ifndef LOGDAEMON_ARRAY_H
#define LOGDAEMON_ARRAY_H

#define ARRAY_NUM_STATIC_ITEMS 32

typedef int (*compare)(const void *, const void *);

#define ARRAY_ELEMENTS(a, i, n) \
void **a;\
unsigned i,n;\
void *static_items[ARRAY_NUM_STATIC_ITEMS]

typedef struct {
    ARRAY_ELEMENTS(a, i, n);
} array;

typedef struct {
    ARRAY_ELEMENTS(a, i, n);
    compare cmp;
} sortArray;

typedef struct {
    array *array;
    unsigned i;
} arrayIterator;

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