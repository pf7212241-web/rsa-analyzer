#include "math_utils.h"
#include <stddef.h>

uint64_t gcd64(uint64_t a,
               uint64_t b) {
    /* Остаток сохраняет множество общих делителей. */
    while (b != 0) {
        uint64_t remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

uint64_t add_mod64(uint64_t a,
                   uint64_t b,
                   uint64_t m) {
    a %= m;
    b %= m;
    /* Сначала вычитаем модуль, если сумма достигла бы его. */
    uint64_t result = a >= m - b ? a - (m - b) : a + b;
    return result;
}

uint64_t mul_mod64(uint64_t a,
                   uint64_t b,
                   uint64_t m) {
    uint64_t result = 0;
    a %= m;
    /* Инвариант: result + a*b сравним с исходным произведением. */
    while (b != 0) {
        if ((b & 1) != 0) {
            result = add_mod64(result, a, m);
        }
        b >>= 1;
        a = add_mod64(a, a, m);
    }
    return result;
}

uint64_t pow_mod64(uint64_t a,
                   uint64_t k,
                   uint64_t m) {
    uint64_t result = 1 % m;
    a %= m;
    /* Двоичное разложение показателя уменьшает число умножений. */
    while (k != 0) {
        if ((k & 1) != 0) {
            result = mul_mod64(result, a, m);
        }
        k >>= 1;
        a = mul_mod64(a, a, m);
    }
    return result;
}

uint64_t integer_sqrt64(uint64_t x) {
    uint64_t low = 0;
    uint64_t high = UINT64_C(4294967295);
    uint64_t result = 0;
    /* Квадрат кандидата заменён делением во избежание переполнения. */
    while (low <= high) {
        uint64_t middle = low + (high - low) / 2;
        if (middle == 0 || middle <= x / middle) {
            result = middle;
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return result;
}

static bool passes_witness(uint64_t n,
                           uint64_t base,
                           uint64_t odd_part,
                           unsigned twos) {
    bool passed = true;
    base %= n;
    if (base != 0) {
        uint64_t value = pow_mod64(base, odd_part, n);
        passed = value == 1 || value == n - 1;
        unsigned step = 1;
        /* Ищем -1 среди последовательных квадратов. */
        while (!passed && step < twos) {
            value = mul_mod64(value, value, n);
            passed = value == n - 1;
            ++step;
        }
    }
    return passed;
}

bool is_prime64(uint64_t n) {
    const uint64_t bases[] = {
        2, 325, 9375, 28178, 450775, 9780504, 1795265022
    };
    bool prime = n >= 2;
    if (n > 2 && n % 2 == 0) {
        prime = false;
    }
    if (prime && n > 2) {
        uint64_t odd_part = n - 1;
        unsigned twos = 0;
        while (odd_part % 2 == 0) {
            odd_part /= 2;
            ++twos;
        }
        /* Набор Sinclair достаточен для всех n < 2^64; см. README. */
        size_t index = 0;
        while (prime && index < sizeof(bases) / sizeof(bases[0])) {
            prime = passes_witness(n, bases[index], odd_part, twos);
            ++index;
        }
    }
    return prime;
}

unsigned bit_length64(uint64_t n) {
    unsigned length = 0;
    while (n != 0) {
        ++length;
        n >>= 1;
    }
    return length;
}
