#include "analysis.h"
#include "factorization.h"
#include "math_utils.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "Ошибка проверки, строка %d: %s\n", __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static bool simple_prime(uint64_t n) {
    bool prime = n >= 2;
    uint64_t divisor = 2;
    while (prime && divisor <= n / divisor) {
        prime = n % divisor != 0;
        ++divisor;
    }
    return prime;
}

int main(void) {
    unsigned checks = 0, failures = 0;
    CHECK(gcd64(0, 0) == 0);
    CHECK(gcd64(UINT64_MAX, 0) == UINT64_MAX);
    CHECK(mul_mod64(UINT64_MAX - 1, UINT64_MAX - 1, UINT64_MAX) == 1);
    CHECK(add_mod64(UINT64_MAX - 1, UINT64_MAX - 1, UINT64_MAX) == UINT64_MAX - 2);
    CHECK(pow_mod64(0, 0, 1) == 0);
    CHECK(integer_sqrt64(UINT64_MAX) == UINT64_C(4294967295));
    CHECK(is_prime64(UINT64_C(18446744073709551557)));
    CHECK(!is_prime64(UINT64_MAX));
    CHECK(!is_prime64(UINT64_C(341550071728321)));
    CHECK(!is_prime64(UINT64_C(3825123056546413051)));
    for (uint64_t n = 0; n < 10000; ++n) {
        CHECK(is_prime64(n) == simple_prime(n));
        uint64_t root = integer_sqrt64(n);
        CHECK(root * root <= n && (root + 1) * (root + 1) > n);
    }
    AnalysisSettings settings = default_settings();
    settings.compare_mode = true;
    settings.pollard_bound = 100;
    for (uint64_t n = 2; n < 500; ++n) {
        FactorizationResult trial = trial_division(n, &settings);
        FactorizationResult fermat = fermat_factor(n, &settings);
        FactorizationResult pollard = pollard_p_minus_one(n, &settings);
        CHECK((trial.status == FOUND) == !simple_prime(n));
        CHECK((fermat.status == FOUND) == !simple_prime(n));
        FactorizationResult results[] = {trial, fermat, pollard};
        for (size_t index = 0; index < 3; ++index) {
            if (results[index].status == FOUND) {
                CHECK(results[index].u > 1 && results[index].v > 1 &&
                      n % results[index].u == 0 &&
                      n / results[index].u == results[index].v);
            }
        }
    }
    FactorizationResult result = fermat_factor(UINT64_MAX, &settings);
    CHECK(result.status == FOUND && result.iterations == 1);
    CHECK(result.u == UINT64_C(4294967295) && result.v == UINT64_C(4294967297));
    result = fermat_factor(UINT64_C(18446743979220271189), &settings);
    CHECK(result.status == FOUND && result.u == UINT64_C(4294967279));
    result = pollard_p_minus_one(261883, &settings);
    CHECK(result.status == FOUND && result.u == 257);
    result = pollard_p_minus_one(15, &settings);
    CHECK(result.status == FOUND && result.u == 3);
    result = pollard_p_minus_one(91, &settings);
    CHECK(result.status == FOUND || result.status == DEGENERATE);
    settings.pollard_base = 90;
    result = pollard_p_minus_one(91, &settings);
    CHECK(result.status == DEGENERATE);
    settings.pollard_base = 91;
    CHECK(pollard_p_minus_one(91, &settings).status == INVALID_INPUT);
    settings.pollard_base = 7;
    CHECK(pollard_p_minus_one(91, &settings).u == 7);
    settings = default_settings();
    settings.trial_bound = 2;
    CHECK(trial_division(77, &settings).reason == BOUND_REACHED);
    settings.trial_bound = 100;
    settings.limits[TRIAL].max_iterations = 1;
    CHECK(trial_division(77, &settings).reason == ITERATION_LIMIT);
    settings.limits[FERMAT].max_iterations = 1;
    CHECK(fermat_factor(303, &settings).reason == ITERATION_LIMIT);
    settings.limits[POLLARD].max_iterations = 1;
    CHECK(pollard_p_minus_one(10403, &settings).reason == ITERATION_LIMIT);
    settings.limits[FERMAT].max_seconds = 1e-300;
    CHECK(fermat_factor(10403, &settings).reason == TIME_LIMIT);
    CHECK(trial_division(0, &settings).status == INVALID_INPUT);
    CHECK(fermat_factor(15, NULL).status == INVALID_INPUT);
    settings = default_settings();
    RsaPublicKey key = {3233, 17};
    ReferenceData ref = {61, 53, 2753, true, true, true};
    AnalysisReport report;
    CHECK(analyze_key(&key, &settings, &ref, &report) == OK);
    CHECK(report.has_factors && report.model_status == CHECK_VALID);
    CHECK(report.control_status == CONTROL_VALID);
    CHECK(report.methods[FERMAT].reason == KNOWN_FACTORS);
    settings.compare_mode = true;
    CHECK(analyze_key(&key, &settings, &ref, &report) == OK);
    CHECK(report.methods[FERMAT].status == FOUND);
    CHECK(report.methods[FERMAT].iterations == fermat_factor(key.n, &settings).iterations);
    ref.d = 1;
    CHECK(analyze_key(&key, &settings, &ref, &report) == OK);
    CHECK(report.control_status == CONTROL_INVALID);
    CHECK(report.model_status == CHECK_VALID);
    ref = (ReferenceData){0, 53, 0, false, true, false};
    CHECK(analyze_key(&key, &settings, &ref, &report) == OK);
    CHECK(report.control_status == CONTROL_VALID);
    ref = (ReferenceData){61, 59, 0, true, true, false};
    CHECK(analyze_key(&key, &settings, &ref, &report) == OK);
    CHECK(report.control_status == CONTROL_INVALID);
    key = (RsaPublicKey){10201, 7};
    CHECK(analyze_key(&key, &settings, NULL, &report) == OK);
    CHECK(report.has_factors && !report.prime_pair && report.model_status == CHECK_INVALID);
    key = (RsaPublicKey){3233, 12};
    CHECK(analyze_key(&key, &settings, NULL, &report) == OK);
    CHECK(report.model_status == CHECK_INVALID);
    settings = default_settings();
    for (size_t index = 0; index < 3; ++index) {
        settings.selected[index] = false;
    }
    key = (RsaPublicKey){3233, 17};
    ref = (ReferenceData){0, 0, 2753, false, false, true};
    CHECK(analyze_key(&key, &settings, &ref, &report) == OK);
    CHECK(report.control_status == CONTROL_DEFERRED);
    RsaPublicKey keys[] = {{3233, 17}, {3599, 17}, {3233, 17}, {15, 3}, {105, 11}};
    BatchReport batch = {0};
    CHECK(analyze_batch(keys, 5, &settings, NULL, &batch) == OK);
    CHECK(batch.reports[0].has_factors && batch.reports[1].has_factors);
    CHECK(batch.pair_count == 4);
    CHECK(batch.pairs[1].kind == DUPLICATE_MODULUS);
    CHECK(batch.reports[4].has_factors && !batch.reports[3].has_factors);
    free_batch_report(&batch);
    free_batch_report(&batch);
    CHECK(batch.count == 0 && batch.pairs == NULL && batch.reports == NULL);
    CHECK(analyze_batch(NULL, 0, &settings, NULL, &batch) == OK);
    free_batch_report(&batch);
    CHECK(analyze_batch(NULL, 1, &settings, NULL, &batch) == INVALID_ARGUMENT);
    CHECK(analyze_batch(keys, RSA_MAX_KEYS + 1, &settings, NULL, &batch) == INVALID_ARGUMENT);
    printf("Проверок: %u; ошибок: %u\n", checks, failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
