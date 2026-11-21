#ifndef MATH_UTILS_H
#define MATH_UTILS_H
#include <stdbool.h>
#include <stdint.h>

uint64_t gcd64(uint64_t a,
               uint64_t b);
/* Предусловие модульных операций: m > 0. */
uint64_t add_mod64(uint64_t a,
                   uint64_t b,
                   uint64_t m);
uint64_t mul_mod64(uint64_t a,
                   uint64_t b,
                   uint64_t m);
uint64_t pow_mod64(uint64_t a,
                   uint64_t k,
                   uint64_t m);
uint64_t integer_sqrt64(uint64_t x);
bool is_prime64(uint64_t n);
unsigned bit_length64(uint64_t n);
#endif
