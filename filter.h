//
// Created by Steve on 7/26/2022.
//

#ifndef LOGD_FILTER_H
#define LOGD_FILTER_H

struct message;
struct varchar;

enum filterType {
    FILTER_NONE = 0,
    FILTER_PID = 1,
    FILTER_LEVEL = 2,
    FILTER_TAG = 3,
};

enum filterOpType {
    FILTER_OP_NONE = 0,
    FILTER_OP_NOT = 1,
    FILTER_OP_AND = 2,
    FILTER_OP_OR = 3,
};

typedef enum filterResult {
    FILTER_RESULT_FALSE = 0,
    FILTER_RESULT_TRUE = 1,
} filterResult;

typedef struct filter {
    enum filterOpType opType;
    enum filterType type;
    union {
        int pid;
        int level;
        struct varchar *tag;
    };
    enum filterResult (*fn)(struct filter *f, struct message *msg);
    struct filter *f2;
} filter;

filter *newPidFilter(int pid);
filter *newLevelFilter(int level);
filter *newTagFilter(const char *tag, int tagLen);
void freeFilter(filter *f);

filterResult evalFilter(filter *f, struct message *msg);
filter *filterNot(filter *f);
filter *filterAnd(filter *f, filter *f2);
filter *filterOr(filter *f, filter *f2);

#endif //LOGD_FILTER_H
