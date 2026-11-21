#include "factorization.h"
#include "math_utils.h"
#include "timing.h"
#include <math.h>

AnalysisSettings default_settings(void) {
    AnalysisSettings settings = {0};
    settings.trial_bound = 100000;
    settings.pollard_bound = 10000;
    settings.pollard_base = 2;
    for (size_t index = 0; index < RSA_METHOD_COUNT; ++index) {
        settings.selected[index] = true;
        settings.limits[index].max_iterations = 100000;
        settings.limits[index].max_seconds = 1.0;
    }
    return settings;
}

bool settings_valid(const AnalysisSettings *settings) {
    bool valid = settings != NULL;
    if (valid) {
        valid = settings->trial_bound >= 2 &&
                settings->pollard_bound >= 2 && settings->pollard_base >= 2;
        size_t index = 0;
        while (valid && index < RSA_METHOD_COUNT) {
            valid = settings->limits[index].max_iterations > 0 &&
                    isfinite(settings->limits[index].max_seconds) &&
                    settings->limits[index].max_seconds > 0;
            ++index;
        }
    }
    return valid;
}

static FactorizationResult initial_result(FactorMethod method) {
    FactorizationResult result = {0};
    result.method = method;
    result.status = SEARCH_COMPLETED;
    result.reason = FULL_SEARCH;
    return result;
}

static bool may_step(FactorizationResult *result,
                     const MethodLimits *limits,
                     double start) {
    double now = monotonic_seconds();
    bool allowed = true;
    /* Проверка выполняется до очередной итерации. */
    if (start < 0 || now < 0) {
        result->status = UNSUPPORTED_RANGE;
        result->reason = CLOCK_ERROR;
        allowed = false;
    } else if (result->iterations >= limits->max_iterations) {
        result->status = LIMIT_REACHED;
        result->reason = ITERATION_LIMIT;
        allowed = false;
    } else if (now - start >= limits->max_seconds) {
        result->status = LIMIT_REACHED;
        result->reason = TIME_LIMIT;
        allowed = false;
    }
    return allowed;
}

static void found(FactorizationResult *result,
                  uint64_t n,
                  uint64_t divisor) {
    /* Каждое найденное разложение подтверждается делением. */
    if (divisor > 1 && divisor < n && n % divisor == 0) {
        result->u = divisor;
        result->v = n / divisor;
        result->status = FOUND;
        result->reason = FACTOR_FOUND;
    }
}

static bool prepare(uint64_t n,
                    const AnalysisSettings *settings,
                    FactorizationResult *result) {
    bool ready = false;
    if (n <= 1 || !settings_valid(settings)) {
        result->status = INVALID_INPUT;
        result->reason = BAD_ARGUMENT;
    } else if (n > 2 && n % 2 == 0) {
        found(result, n, 2);
    } else if (n > 2) {
        ready = true;
    }
    return ready;
}

static void finish(FactorizationResult *result,
                   double start) {
    double now = monotonic_seconds();
    if (start >= 0 && now >= start) {
        result->elapsed = now - start;
    } else {
        result->elapsed = -1;
    }
}

FactorizationResult trial_division(uint64_t n,
                                   const AnalysisSettings *settings) {
    FactorizationResult result = initial_result(TRIAL);
    double start = monotonic_seconds();
    bool running = prepare(n, settings, &result);
    uint64_t divisor = 3;
    /* Проверка чётности — подготовка, итерация — нечётный кандидат. */
    while (running && divisor <= n / divisor) {
        if (divisor > settings->trial_bound) {
            result.reason = BOUND_REACHED;
            running = false;
        } else if (!may_step(&result, &settings->limits[TRIAL], start)) {
            running = false;
        } else {
            ++result.iterations;
            if (n % divisor == 0) {
                found(&result, n, divisor);
                running = false;
            } else {
                divisor += 2;
            }
        }
    }
    finish(&result, start);
    return result;
}

FactorizationResult fermat_factor(uint64_t n,
                                  const AnalysisSettings *settings) {
    FactorizationResult result = initial_result(FERMAT);
    double start = monotonic_seconds();
    bool running = prepare(n, settings, &result);
    uint64_t a = 0, difference = 0;
    if (running) {
        uint64_t root = integer_sqrt64(n);
        uint64_t gap = n - root * root;
        a = root;
        if (gap != 0) {
            a = root + 1;
            /* Даже при a=2^32 не вычисляем переполняющийся квадрат. */
            difference = 2 * root + 1 - gap;
        }
    }
    while (running) {
        if (!may_step(&result, &settings->limits[FERMAT], start)) {
            running = false;
        } else {
            ++result.iterations;
            uint64_t b = integer_sqrt64(difference);
            if (b * b == difference) {
                found(&result, n, a - b);
                /* Пара (1,n) означает исчерпание поиска для простого n. */
                running = false;
            } else if (a > (UINT64_MAX - 1) / 2 ||
                       difference > UINT64_MAX - (2 * a + 1)) {
                result.status = UNSUPPORTED_RANGE;
                result.reason = ARITHMETIC_LIMIT;
                running = false;
            } else {
                /* (a+1)^2-n = (a^2-n) + 2a+1. */
                difference += 2 * a + 1;
                ++a;
            }
        }
    }
    finish(&result, start);
    return result;
}

FactorizationResult pollard_p_minus_one(uint64_t n,
                                        const AnalysisSettings *settings) {
    FactorizationResult result = initial_result(POLLARD);
    double start = monotonic_seconds();
    bool running = prepare(n, settings, &result);
    uint64_t value = 0, step = 2;
    if (running) {
        if (settings->pollard_base >= n) {
            result.status = INVALID_INPUT;
            result.reason = BAD_BASE;
            running = false;
        } else {
            value = settings->pollard_base;
            uint64_t common = gcd64(value, n);
            if (common > 1) {
                found(&result, n, common);
                running = false;
            }
        }
    }
    while (running) {
        if (!may_step(&result, &settings->limits[POLLARD], start)) {
            running = false;
        } else {
            ++result.iterations;
            /* Накопленный показатель равен step!, но не хранится. */
            value = pow_mod64(value, step, n);
            uint64_t residue = value == 0 ? n - 1 : value - 1;
            uint64_t common = gcd64(residue, n);
            if (common > 1 && common < n) {
                found(&result, n, common);
                running = false;
            } else if (common == n) {
                result.status = DEGENERATE;
                result.reason = DEGENERATE_GCD;
                running = false;
            } else if (step == settings->pollard_bound) {
                result.reason = BOUND_REACHED;
                running = false;
            } else {
                ++step;
            }
        }
    }
    finish(&result, start);
    return result;
}
