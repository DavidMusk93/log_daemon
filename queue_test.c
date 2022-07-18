//
// Created by Steve on 7/18/2022.
//

#include <stdio.h>

#include "queue.h"
#include "macro.h"

struct foo {
    int x;
    queueEntry entry;
};

_main() {
    queueEntry head = {};
    initQueue(&head);

    struct foo a = {.x=1}, b = {.x=2}, c = {.x=3};
    struct foo *pointerExpect[] = {&c, &b, &a};
    int indexExpect = 0;

    pushQueue(&head, &a.entry);
    pushQueue(&head, &b.entry);
    pushQueue(&head, &c.entry);

    foreachQueue(&head, iter) {
        struct foo *ptr = (void *) iter - offsetOf(struct foo, entry);
        if (ptr != pointerExpect[indexExpect++]) {
            return 1;
        }
    }
    return 0;
}
