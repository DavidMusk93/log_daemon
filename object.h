//
// Created by Steve on 7/19/2022.
//

#ifndef LOGD_OBJECT_H
#define LOGD_OBJECT_H

#include <stddef.h>

typedef void (*disposeFn)(void *objectPointer, void *context);
typedef struct arena arena;

arena *newArena();
void freeArena(void *arena);
void *claimObject(arena *arena, size_t objectSize);
void reclaimObject(arena *arena, void *objectPointer);

/* operation pair on Object:
 *   make & unref
 *   ref & unref
 */
void *makeObject(size_t objectSize, disposeFn dispose, void *context);
int refObject(void *objectPointer);
int unrefObject(void *objectPointer);


#endif //LOGD_OBJECT_H
