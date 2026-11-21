#include "input_output.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Описание ABI проверяется Python до чтения структур из DLL. */
size_t gui_layout(size_t index) {
    const size_t layout[] = {
        sizeof(RsaPublicKey), offsetof(RsaPublicKey, n), offsetof(RsaPublicKey, e),
        sizeof(ReferenceData), offsetof(ReferenceData, p), offsetof(ReferenceData, q),
        offsetof(ReferenceData, d), offsetof(ReferenceData, has_p),
        offsetof(ReferenceData, has_q), offsetof(ReferenceData, has_d),
        sizeof(MethodLimits), offsetof(MethodLimits, max_iterations), offsetof(MethodLimits, max_seconds),
        sizeof(AnalysisSettings), offsetof(AnalysisSettings, trial_bound),
        offsetof(AnalysisSettings, pollard_bound), offsetof(AnalysisSettings, pollard_base),
        offsetof(AnalysisSettings, selected), offsetof(AnalysisSettings, limits),
        offsetof(AnalysisSettings, compare_mode),
        sizeof(FactorizationResult), offsetof(FactorizationResult, method),
        offsetof(FactorizationResult, status), offsetof(FactorizationResult, reason),
        offsetof(FactorizationResult, u), offsetof(FactorizationResult, v),
        offsetof(FactorizationResult, iterations), offsetof(FactorizationResult, elapsed),
        sizeof(CommonFactorRecord), offsetof(CommonFactorRecord, first),
        offsetof(CommonFactorRecord, second), offsetof(CommonFactorRecord, gcd), offsetof(CommonFactorRecord, kind),
        sizeof(AnalysisReport), offsetof(AnalysisReport, key), offsetof(AnalysisReport, bit_length),
        offsetof(AnalysisReport, methods), offsetof(AnalysisReport, has_factors),
        offsetof(AnalysisReport, prime_pair), offsetof(AnalysisReport, u), offsetof(AnalysisReport, v),
        offsetof(AnalysisReport, model_status), offsetof(AnalysisReport, control_status),
        sizeof(BatchReport), offsetof(BatchReport, reports), offsetof(BatchReport, count),
        offsetof(BatchReport, pairs), offsetof(BatchReport, pair_count),
        sizeof(InputRecord), offsetof(InputRecord, line_number), offsetof(InputRecord, report_index),
        offsetof(InputRecord, kind), offsetof(InputRecord, key), offsetof(InputRecord, ref),
        offsetof(InputRecord, n), offsetof(InputRecord, e), offsetof(InputRecord, error),
        offsetof(InputRecord, n_bits), offsetof(InputRecord, e_bits),
        sizeof(AppData), offsetof(AppData, records), offsetof(AppData, count), offsetof(AppData, batch)
    };
    size_t value = (size_t)-1;
    if (index < sizeof(layout) / sizeof(layout[0])) {
        value = layout[index];
    }
    return value;
}

/* out обнулён. Даже после ошибки вызывающая сторона выполняет cleanup. */
int gui_load(const char *source,
             int from_file,
             AppData *out) {
    int code = 1;
    if (source != NULL && out != NULL && out->records == NULL &&
        out->batch.reports == NULL && out->batch.pairs == NULL && out->count == 0) {
        if (from_file) {
            FILE *stream = open_text(source, "rb");
            if (stream != NULL) {
                bool loaded = read_input(stream, out);
                if (fclose(stream) != 0) {
                    loaded = false;
                }
                code = loaded ? 0 : 1;
            }
        } else if (strlen(source) < MAX_LINE) {
            out->records = calloc(1, sizeof(*out->records));
            if (out->records != NULL) {
                char line[MAX_LINE];
                strcpy(line, source);
                out->count = 1;
                out->records[0].line_number = 1;
                validate_input(line, &out->records[0]);
                code = 0;
            }
        }
    }
    return code;
}

/* Поток создаётся и закрывается внутри той же библиотеки C. */
int gui_write_report(const char *path,
                     const AppData *data,
                     const AnalysisSettings *settings) {
    int code = 1;
    if (path != NULL && data != NULL && settings != NULL) {
        FILE *stream = open_text(path, "wx");
        if (stream != NULL) {
            bool written = write_report(stream, data, settings);
            if (fclose(stream) != 0) {
                written = false;
            }
            code = written ? 0 : 1;
        }
    }
    return code;
}
