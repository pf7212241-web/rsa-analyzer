#ifndef RSA_TYPES_H
#define RSA_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RSA_METHOD_COUNT 3
#define RSA_MAX_KEYS 256

typedef enum { TRIAL, FERMAT, POLLARD } FactorMethod;
typedef enum {
    FOUND, SEARCH_COMPLETED, LIMIT_REACHED, DEGENERATE,
    INVALID_INPUT, UNSUPPORTED_RANGE, NOT_RUN
} FactorStatus;
typedef enum {
    FACTOR_FOUND, FULL_SEARCH, BOUND_REACHED, ITERATION_LIMIT,
    TIME_LIMIT, ARITHMETIC_LIMIT, BAD_ARGUMENT, BAD_BASE,
    DEGENERATE_GCD, DISABLED, KNOWN_FACTORS, CLOCK_ERROR
} StopReason;
typedef enum { OK, INVALID_ARGUMENT, MEMORY_ERROR } AnalysisStatus;
typedef enum { CHECK_UNKNOWN, CHECK_VALID, CHECK_INVALID } CheckStatus;
typedef enum { CONTROL_NONE, CONTROL_DEFERRED, CONTROL_VALID,
               CONTROL_INVALID } ControlStatus;
typedef enum { SHARED_FACTOR, DUPLICATE_MODULUS,
               DIVISIBLE_MODULUS } PairKind;

typedef struct { uint64_t n, e; } RsaPublicKey;
typedef struct {
    uint64_t p, q, d;
    bool has_p, has_q, has_d;
} ReferenceData;
typedef struct {
    uint64_t max_iterations;
    double max_seconds;
} MethodLimits;
typedef struct {
    uint64_t trial_bound, pollard_bound, pollard_base;
    bool selected[RSA_METHOD_COUNT];
    MethodLimits limits[RSA_METHOD_COUNT];
    bool compare_mode;
} AnalysisSettings;
typedef struct {
    FactorMethod method;
    FactorStatus status;
    StopReason reason;
    uint64_t u, v, iterations;
    double elapsed;
} FactorizationResult;
typedef struct {
    size_t first, second;
    uint64_t gcd;
    PairKind kind;
} CommonFactorRecord;
typedef struct {
    RsaPublicKey key;
    unsigned bit_length;
    FactorizationResult methods[RSA_METHOD_COUNT];
    bool has_factors, prime_pair;
    uint64_t u, v;
    CheckStatus model_status;
    ControlStatus control_status;
} AnalysisReport;
typedef struct {
    AnalysisReport *reports;
    size_t count;
    CommonFactorRecord *pairs;
    size_t pair_count;
} BatchReport;

#endif
