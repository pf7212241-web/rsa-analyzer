#include "math_utils.h"
#include <inttypes.h>
#include <stdio.h>

/* Машинный протокол только для независимой проверки Python. */
int main(void) {
    uint64_t a, b, m;
    while (scanf("%" SCNu64 " %" SCNu64 " %" SCNu64, &a, &b, &m) == 3) {
        if (m > 0) {
            printf("%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %u\n",
                    gcd64(a, b), mul_mod64(a, b, m), pow_mod64(a, b, m),
                    integer_sqrt64(a), bit_length64(a));
        }
    }
    return 0;
}
