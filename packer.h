//
// Created by Steve on 7/28/2022.
//

#ifndef LOGD_PACKER_H
#define LOGD_PACKER_H

#include "defs.h"

typedef struct byteWriter byteWriter;
typedef struct byteReader byteReader;

struct logTag;
struct filter;

byteWriter *newByteWriter(int capacity);
void freeByteWriter(void *ptrPtr);
byteWriter *putData(struct byteWriter *w, const void *buf, int bufLen, int *linkOff);
void *dataPtr(struct byteWriter *w, int *ptrLen);

int getData(byteReader *r, void *buf, int bufLen);

struct packerFilter {
    int filterCount;
    int indexCount;
    byteWriter *w;
};

void initPackerFilter(struct packerFilter *o);
void *finalizePackerFilter(struct packerFilter *o, int *ptrLen);
void freePackerFilter(void *ptr);


int packPid(struct packerFilter *o, int pid);
int packLevel(struct packerFilter *o, int level);
int packTag(struct packerFilter *o, const char *tag, int tagLen);

struct byteReader {
    cstr s, e;
};
int unpackFilter(byteReader *r, struct filter **ptrFilter);

#endif //LOGD_PACKER
