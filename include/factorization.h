#ifndef FACTORIZATION_H
#define FACTORIZATION_H
#include "rsa_types.h"

AnalysisSettings default_settings(void);
bool settings_valid(const AnalysisSettings *settings);
FactorizationResult trial_division(uint64_t n,
                                   const AnalysisSettings *settings);
FactorizationResult fermat_factor(uint64_t n,
                                  const AnalysisSettings *settings);
FactorizationResult pollard_p_minus_one(uint64_t n,
                                        const AnalysisSettings *settings);
#endif
