#ifndef ANALYSIS_H
#define ANALYSIS_H
#include "rsa_types.h"

/* Выход analyze_key не владеет динамической памятью. ref может быть NULL. */
AnalysisStatus analyze_key(const RsaPublicKey *key,
                           const AnalysisSettings *settings,
                           const ReferenceData *ref,
                           AnalysisReport *out);
/* out заранее содержит count и массив инициализированных reports.
 * pairs должен быть NULL. Ключи должны соответствовать reports. */
AnalysisStatus check_common_factors(const RsaPublicKey *keys,
                                    size_t count,
                                    BatchReport *out);
/* Пустой набор допустим. count <= RSA_MAX_KEYS.
 * out обнулён или освобождён. refs: NULL либо массив длины count.
 * При OK вызывающий обязан выполнить free_batch_report. */
AnalysisStatus analyze_batch(const RsaPublicKey *keys,
                             size_t count,
                             const AnalysisSettings *settings,
                             const ReferenceData *refs,
                             BatchReport *out);
void free_batch_report(BatchReport *report);
#endif
