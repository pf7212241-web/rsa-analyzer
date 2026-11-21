#include "analysis.h"
#include "factorization.h"
#include "math_utils.h"
#include <stdlib.h>

static bool key_valid(const RsaPublicKey *key) {
    bool valid = key != NULL;
    if (valid) {
        valid = key->n > 1 && key->e > 1 && key->e < key->n;
    }
    return valid;
}

static void accept_factor(AnalysisReport *report,
                          uint64_t divisor) {
    uint64_t n = report->key.n;
    if (divisor > 1 && divisor < n && n % divisor == 0) {
        uint64_t other = n / divisor;
        report->has_factors = true;
        report->u = divisor < other ? divisor : other;
        report->v = n / report->u;
        report->prime_pair = report->u != report->v &&
                            is_prime64(report->u) && is_prime64(report->v);
        report->model_status = CHECK_INVALID;
        if (report->prime_pair && report->u > 2 && report->v > 2) {
            /* Произведение меньше n, поскольку делители подтверждены. */
            uint64_t phi = (report->u - 1) * (report->v - 1);
            if (gcd64(report->key.e, phi) == 1) {
                report->model_status = CHECK_VALID;
            }
        }
    }
}

static void initialize_report(const RsaPublicKey *key,
                              AnalysisReport *report) {
    *report = (AnalysisReport){0};
    report->key = *key;
    report->bit_length = bit_length64(key->n);
    for (size_t index = 0; index < RSA_METHOD_COUNT; ++index) {
        report->methods[index].method = (FactorMethod)index;
        report->methods[index].status = NOT_RUN;
        report->methods[index].reason = DISABLED;
    }
    /* Диагностика простоты не подменяет независимые запуски методов. */
    if (is_prime64(key->n) || key->n % 2 == 0) {
        report->model_status = CHECK_INVALID;
    }
}

static void check_reference(AnalysisReport *report,
                            const ReferenceData *ref) {
    bool present = ref != NULL && (ref->has_p || ref->has_q || ref->has_d);
    if (present) {
        bool valid = !ref->has_d || ref->d > 0;
        uint64_t n = report->key.n;
        if (ref->has_p || ref->has_q) {
            uint64_t divisor = ref->has_p ? ref->p : ref->q;
            bool divides = divisor > 1 && divisor < n && n % divisor == 0;
            if (divides) {
                uint64_t other = n / divisor;
                valid = valid && (!ref->has_p || !ref->has_q ||
                                  other == ref->q);
                valid = valid && divisor > 2 && other > 2 &&
                        divisor != other && is_prime64(divisor) &&
                        is_prime64(other);
                /* Даже неверная контрольная пара может раскрыть делитель. */
                accept_factor(report, divisor);
            } else {
                valid = false;
            }
        }
        if (!valid) {
            report->control_status = CONTROL_INVALID;
        } else if (!report->has_factors) {
            report->control_status = CONTROL_DEFERRED;
        } else {
            valid = report->model_status == CHECK_VALID;
            if (valid && ref->has_d) {
                uint64_t phi = (report->u - 1) * (report->v - 1);
                valid = mul_mod64(report->key.e, ref->d, phi) == 1;
            }
            report->control_status = valid ? CONTROL_VALID : CONTROL_INVALID;
        }
    }
}

static void run_methods(AnalysisReport *report,
                        const AnalysisSettings *settings,
                        const ReferenceData *ref) {
    check_reference(report, ref);
    for (size_t index = 0; index < RSA_METHOD_COUNT; ++index) {
        if (settings->selected[index]) {
            if (report->has_factors && !settings->compare_mode) {
                report->methods[index].reason = KNOWN_FACTORS;
            } else {
                FactorizationResult result;
                if (index == TRIAL) {
                    result = trial_division(report->key.n, settings);
                } else if (index == FERMAT) {
                    result = fermat_factor(report->key.n, settings);
                } else {
                    result = pollard_p_minus_one(report->key.n, settings);
                }
                report->methods[index] = result;
                if (result.status == FOUND) {
                    accept_factor(report, result.u);
                }
            }
        }
    }
    /* Закрытая экспонента проверяется и после восстановления множителей. */
    check_reference(report, ref);
}

AnalysisStatus analyze_key(const RsaPublicKey *key,
                           const AnalysisSettings *settings,
                           const ReferenceData *ref,
                           AnalysisReport *out) {
    AnalysisStatus status = INVALID_ARGUMENT;
    if (out != NULL && key_valid(key) && settings_valid(settings)) {
        initialize_report(key, out);
        run_methods(out, settings, ref);
        status = OK;
    }
    return status;
}

AnalysisStatus check_common_factors(const RsaPublicKey *keys,
                                    size_t count,
                                    BatchReport *out) {
    AnalysisStatus status = INVALID_ARGUMENT;
    bool valid = out != NULL && count <= RSA_MAX_KEYS;
    if (valid) {
        valid = out->count == count && out->pairs == NULL &&
                (count == 0 || (keys != NULL && out->reports != NULL));
    }
    size_t index = 0;
    while (valid && index < count) {
        valid = key_valid(&keys[index]) &&
                out->reports[index].key.n == keys[index].n &&
                out->reports[index].key.e == keys[index].e;
        ++index;
    }
    if (valid) {
        status = OK;
        out->pair_count = 0;
        size_t capacity = count > 1 ? count * (count - 1) / 2 : 0;
        if (capacity > 0) {
            out->pairs = calloc(capacity, sizeof(*out->pairs));
            if (out->pairs == NULL) {
                status = MEMORY_ERROR;
            }
        }
        /* Сохраняем лишь связанные пары, не все взаимно простые модули. */
        for (size_t first = 0; status == OK && first < count; ++first) {
            for (size_t second = first + 1; second < count; ++second) {
                uint64_t common = gcd64(keys[first].n, keys[second].n);
                if (common > 1) {
                    CommonFactorRecord record = {first, second, common,
                                                  SHARED_FACTOR};
                    if (keys[first].n == keys[second].n) {
                        record.kind = DUPLICATE_MODULUS;
                    } else if (common == keys[first].n ||
                               common == keys[second].n) {
                        record.kind = DIVISIBLE_MODULUS;
                    }
                    out->pairs[out->pair_count++] = record;
                    accept_factor(&out->reports[first], common);
                    accept_factor(&out->reports[second], common);
                }
            }
        }
    }
    return status;
}

AnalysisStatus analyze_batch(const RsaPublicKey *keys,
                             size_t count,
                             const AnalysisSettings *settings,
                             const ReferenceData *refs,
                             BatchReport *out) {
    AnalysisStatus status = INVALID_ARGUMENT;
    bool valid = out != NULL && count <= RSA_MAX_KEYS &&
                 settings_valid(settings) && (count == 0 || keys != NULL);
    if (valid) {
        valid = out->reports == NULL && out->pairs == NULL &&
                out->count == 0 && out->pair_count == 0;
    }
    size_t index = 0;
    while (valid && index < count) {
        valid = key_valid(&keys[index]);
        ++index;
    }
    if (valid) {
        status = OK;
        if (count > 0) {
            out->reports = calloc(count, sizeof(*out->reports));
            if (out->reports == NULL) {
                status = MEMORY_ERROR;
            }
        }
        if (status == OK) {
            out->count = count;
            for (index = 0; index < count; ++index) {
                initialize_report(&keys[index], &out->reports[index]);
            }
            status = check_common_factors(keys, count, out);
        }
        if (status == OK) {
            for (index = 0; index < count; ++index) {
                const ReferenceData *ref = refs == NULL ? NULL : &refs[index];
                run_methods(&out->reports[index], settings, ref);
            }
        } else {
            free_batch_report(out);
        }
    }
    return status;
}

void free_batch_report(BatchReport *report) {
    if (report != NULL) {
        /* Освобождаем только массивы, которыми владеет отчёт. */
        free(report->reports);
        free(report->pairs);
        *report = (BatchReport){0};
    }
}
