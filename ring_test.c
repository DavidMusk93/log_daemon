//
// Created by Steve on 7/18/2022.
//

#include "array.h"
#include "macro.h"

_main() {
    int a[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    struct ringArray *ring = newRingArray(3);
    int i;
    for (i = 0; i < dimensionOf(a); i++) {
        pushRingArray(ring, &a[i]);
    }
    int *r;
    int indexExpect = dimensionOf(a) - ring->total;
    while ((r = popRingArray(ring))) {
        if (*r != a[indexExpect++]) {
            return 1;
        }
    }
    /* TODO: free ringArray* */
    return 0;
}
