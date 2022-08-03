//
// Created by Steve on 7/18/2022.
//

#ifndef LOGD_QUEUE_H
#define LOGD_QUEUE_H

typedef void *queueEntry[2];

#define prevQueueEntry(_e) ((queueEntry*)(*(_e))[0])
#define nextQueueEntry(_e) ((queueEntry*)(*(_e))[1])
#define refPrevQueueEntry(_e) (*(queueEntry**)(_e))
#define refNextQueueEntry(_e) (*(queueEntry**)&(*(_e))[1])

#define initQueue(_h) \
refNextQueueEntry(_h)=_h;\
refPrevQueueEntry(_h)=_h

#define emptyQueue(_h) \
((_h)==nextQueueEntry(_h))

#define pushQueue(_h, _e) \
refPrevQueueEntry(nextQueueEntry(_h))=_e;\
refNextQueueEntry(_e)=nextQueueEntry(_h);\
refNextQueueEntry(_h)=_e;\
refPrevQueueEntry(_e)=_h

#define popQueue(_h, _e) \
(_e)=prevQueueEntry(_h);\
removeQueueEntry(_e)

#define removeQueueEntry(_e) \
refNextQueueEntry(prevQueueEntry(_e))=nextQueueEntry(_e);\
refPrevQueueEntry(nextQueueEntry(_e))=prevQueueEntry(_e)

#define foreachQueue(_h, _i) \
for(queueEntry*_i=nextQueueEntry(_h);_i!=(_h);_i=nextQueueEntry(_i))

#endif //LOGD_QUEUE_H
