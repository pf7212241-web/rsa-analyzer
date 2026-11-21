#ifndef INPUT_OUTPUT_H
#define INPUT_OUTPUT_H
#include "analysis.h"
#include <stdio.h>

#define MAX_DIGITS 1024
#define MAX_LINE 5200
typedef enum { RECORD_KEY, RECORD_LARGE, RECORD_ERROR } RecordKind;
typedef struct {
    size_t line_number, report_index;
    RecordKind kind;
    RsaPublicKey key;
    ReferenceData ref;
    char n[MAX_DIGITS + 1], e[MAX_DIGITS + 1];
    char error[256];
    unsigned n_bits, e_bits;
} InputRecord;
typedef struct {
    InputRecord *records;
    size_t count;
    BatchReport batch;
} AppData;

/* stream и buffer действительны, capacity >= 1.
 * 1: строка; 0: конец; -1: длина/нулевой байт; -2: ошибка чтения. */
int read_line(FILE *stream,
              char *buffer,
              size_t capacity);
bool parse_u64(const char *text,
               uint64_t *value);
bool parse_positive_seconds(const char *text,
                            double *value);
bool read_input(FILE *stream,
                AppData *data);
void validate_input(char *line,
                    InputRecord *record);
void describe_large_keys(AppData *data);
AnalysisStatus analyze_input(AppData *data,
                             const AnalysisSettings *settings);
bool write_report(FILE *stream,
                  const AppData *data,
                  const AnalysisSettings *settings);
void cleanup(AppData *data);
/* Файлы в UTF-8; пути UTF-8, включая Windows. */
FILE *open_text(const char *path,
                const char *mode);
#endif
