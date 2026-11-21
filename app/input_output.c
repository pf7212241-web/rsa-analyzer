#include "input_output.h"
#include "factorization.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

FILE *open_text(const char *path,
                const char *mode) {
    FILE *stream = NULL;
#ifdef _WIN32
    wchar_t wide_path[32768];
    wchar_t wide_mode[8];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                            wide_path, 32768) > 0 &&
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1,
                            wide_mode, 8) > 0) {
        stream = _wfopen(wide_path, wide_mode);
    }
#else
    stream = fopen(path, mode);
#endif
    return stream;
}

int read_line(FILE *stream,
              char *buffer,
              size_t capacity) {
    size_t length = 0;
    bool too_long = false, has_null = false;
    int value = fgetc(stream);
    bool has_data = value != EOF;
    /* Остаток длинной строки дочитывается, не становится новой записью. */
    while (value != EOF && value != '\n') {
        if (value == 0) {
            has_null = true;
        }
        if (length + 1 < capacity) {
            buffer[length++] = (char)value;
        } else {
            too_long = true;
        }
        value = fgetc(stream);
    }
    buffer[length] = '\0';
    if (length > 0 && buffer[length - 1] == '\r') {
        buffer[length - 1] = '\0';
    }
    int status = has_data ? 1 : 0;
    if (too_long || has_null) {
        status = -1;
    }
    if (ferror(stream)) {
        status = -2;
    }
    return status;
}

bool parse_u64(const char *text,
               uint64_t *value) {
    bool valid = text != NULL && value != NULL && text[0] != '\0';
    uint64_t number = 0;
    size_t index = 0;
    while (valid && text[index] != '\0') {
        if (text[index] < '0' || text[index] > '9') {
            valid = false;
        } else {
            unsigned digit = (unsigned)(text[index] - '0');
            if (number > (UINT64_MAX - digit) / 10) {
                valid = false;
            } else {
                number = number * 10 + digit;
            }
        }
        ++index;
    }
    if (valid) {
        *value = number;
    }
    return valid;
}

bool parse_positive_seconds(const char *text,
                            double *value) {
    char *end = NULL;
    errno = 0;
    double number = strtod(text, &end);
    bool valid = end != text && *end == '\0' && errno == 0 &&
                 isfinite(number) && number > 0;
    if (valid) {
        *value = number;
    }
    return valid;
}

static bool decimal_text(const char *text) {
    size_t length = strlen(text);
    bool valid = length > 0 && length <= MAX_DIGITS;
    for (size_t index = 0; valid && index < length; ++index) {
        valid = text[index] >= '0' && text[index] <= '9';
    }
    return valid;
}

static const char *without_zeros(const char *text) {
    while (text[0] == '0' && text[1] != '\0') {
        ++text;
    }
    return text;
}

void validate_input(char *line,
                    InputRecord *record) {
    char *fields[6] = {0};
    size_t count = 0;
    char *cursor = line;
    record->kind = RECORD_ERROR;
    /* Разделяем строку без strtok и скрытого состояния. */
    while (*cursor != '\0' && count < 6) {
        while (isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        if (*cursor == '#') {
            *cursor = '\0';
        }
        if (*cursor != '\0') {
            fields[count++] = cursor;
            while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
                ++cursor;
            }
            if (*cursor != '\0') {
                *cursor++ = '\0';
            }
        }
    }
    bool valid = count == 2 || count == 5;
    if (valid) {
        valid = decimal_text(fields[0]) && decimal_text(fields[1]);
    }
    if (valid) {
        strcpy(record->n, without_zeros(fields[0]));
        strcpy(record->e, without_zeros(fields[1]));
        valid = strcmp(record->n, "0") != 0 && strcmp(record->e, "0") != 0;
    }
    if (valid && count == 5) {
        uint64_t *values[] = {&record->ref.p, &record->ref.q, &record->ref.d};
        bool *flags[] = {&record->ref.has_p, &record->ref.has_q,
                         &record->ref.has_d};
        for (size_t index = 0; index < 3; ++index) {
            if (strcmp(fields[index + 2], "-") != 0) {
                *flags[index] = true;
                valid = parse_u64(fields[index + 2], values[index]) && valid;
            }
        }
    }
    if (!valid) {
        strcpy(record->error, "Ожидаются n e либо n e p q d; числа без знака, отсутствующие параметры — дефис.");
    } else {
        bool n_fits = parse_u64(record->n, &record->key.n);
        bool e_fits = parse_u64(record->e, &record->key.e);
        if (!n_fits || !e_fits) {
            record->kind = RECORD_LARGE;
        } else if (record->key.n <= 1 || record->key.e <= 1 ||
                   record->key.e >= record->key.n) {
            strcpy(record->error, "Нужны n > 1 и 1 < e < n.");
        } else {
            record->kind = RECORD_KEY;
        }
    }
}

bool read_input(FILE *stream,
                AppData *data) {
    char line[MAX_LINE];
    bool success = data != NULL && stream != NULL;
    size_t line_number = 0;
    if (success) {
        data->records = calloc(RSA_MAX_KEYS, sizeof(*data->records));
        success = data->records != NULL;
    }
    int status = 1;
    while (success && status != 0) {
        status = read_line(stream, line, sizeof(line));
        ++line_number;
        char *start = line;
        if (line_number == 1 && strlen(start) >= 3 &&
            (unsigned char)start[0] == 0xef &&
            (unsigned char)start[1] == 0xbb &&
            (unsigned char)start[2] == 0xbf) {
            start += 3;
        }
        while (isspace((unsigned char)*start)) {
            ++start;
        }
        if (status == -2) {
            success = false;
        } else if (status == -1 || (status == 1 && *start != '\0' &&
                                    *start != '#')) {
            if (data->count == RSA_MAX_KEYS) {
                success = false;
            } else {
                InputRecord *record = &data->records[data->count++];
                record->line_number = line_number;
                if (status == -1) {
                    record->kind = RECORD_ERROR;
                    strcpy(record->error, "Слишком длинная строка или нулевой байт во входе.");
                } else {
                    validate_input(start, record);
                }
            }
        }
    }
    return success;
}

static unsigned decimal_bits(const char *text) {
    char digits[MAX_DIGITS + 1];
    strcpy(digits, text);
    size_t offset = 0, length = strlen(digits);
    unsigned bits = 0;
    /* Деление десятичной записи на два; хранится только одна строка. */
    while (offset < length) {
        unsigned remainder = 0;
        for (size_t index = offset; index < length; ++index) {
            unsigned number = remainder * 10 + (unsigned)(digits[index] - '0');
            digits[index] = (char)('0' + number / 2);
            remainder = number % 2;
        }
        while (offset < length && digits[offset] == '0') {
            ++offset;
        }
        ++bits;
    }
    return bits;
}

void describe_large_keys(AppData *data) {
    for (size_t index = 0; index < data->count; ++index) {
        InputRecord *record = &data->records[index];
        if (record->kind == RECORD_LARGE) {
            record->n_bits = decimal_bits(record->n);
            record->e_bits = decimal_bits(record->e);
        }
    }
}

AnalysisStatus analyze_input(AppData *data,
                             const AnalysisSettings *settings) {
    RsaPublicKey keys[RSA_MAX_KEYS];
    ReferenceData refs[RSA_MAX_KEYS];
    size_t count = 0;
    for (size_t index = 0; index < data->count; ++index) {
        InputRecord *record = &data->records[index];
        if (record->kind == RECORD_KEY) {
            record->report_index = count;
            keys[count] = record->key;
            refs[count++] = record->ref;
        }
    }
    AnalysisStatus status = analyze_batch(keys, count, settings, refs,
                                          &data->batch);
    return status;
}

static const char *status_name(FactorStatus status) {
    const char *names[] = {"Разложение найдено", "Поиск завершён",
        "Достигнут лимит", "Вырожденный результат", "Ошибка параметров",
        "Операция недоступна", "Не запускался"};
    return names[status];
}

static const char *reason_name(StopReason reason) {
    const char *names[] = {"подтверждены делители", "полный поиск",
        "достигнута граница метода", "лимит итераций", "лимит времени",
        "граница арифметики", "недопустимые параметры", "основание не меньше n",
        "НОД равен n", "не выбран", "разложение уже известно",
        "ошибка часов"};
    return names[reason];
}

static void write_key(FILE *stream,
                      const AnalysisReport *report) {
    const char *methods[] = {"Пробное деление", "Ферма", "Поллард p-1"};
    const char *models[] = {"не установлено", "соответствует", "не соответствует"};
    const char *controls[] = {"не заданы", "отложена до факторизации",
                              "подтверждены в учебной модели", "не согласованы с учебной моделью"};
    fprintf(stream, "Разрядность n: %u. Учебная модель: %s.\n",
            report->bit_length, models[report->model_status]);
    fprintf(stream, "Контрольные параметры: %s.\n", controls[report->control_status]);
    if (report->has_factors) {
        fprintf(stream, "Подтверждённое разложение: %" PRIu64 " * %" PRIu64
                ". Различные простые множители: %s.\n", report->u,
                report->v, report->prime_pair ? "да" : "нет");
    } else {
        fprintf(stream, "Разложение не найдено. Это не доказывает стойкость ключа.\n");
    }
    for (size_t index = 0; index < RSA_METHOD_COUNT; ++index) {
        const FactorizationResult *result = &report->methods[index];
        fprintf(stream, "  %s: %s; %s; итерации=%" PRIu64 "; время=%.6f с",
                methods[index], status_name(result->status),
                reason_name(result->reason), result->iterations, result->elapsed);
        if (result->status == FOUND) {
            fprintf(stream, "; %" PRIu64 " * %" PRIu64, result->u, result->v);
        }
        fputc('\n', stream);
    }
}

bool write_report(FILE *stream,
                  const AppData *data,
                  const AnalysisSettings *settings) {
    const char *methods[] = {"Пробное деление", "Ферма", "Поллард p-1"};
    fprintf(stream, "ОЦЕНКА RSA — УЧЕБНЫЙ АНАЛИЗ\n");
    fprintf(stream, "Режим: %s. D=%" PRIu64 ", B=%" PRIu64 ", основание=%" PRIu64 "\n",
            settings->compare_mode ? "независимое сравнение" : "обычный",
            settings->trial_bound, settings->pollard_bound, settings->pollard_base);
    for (size_t index = 0; index < RSA_METHOD_COUNT; ++index) {
        fprintf(stream, "%s: выбран=%s; лимит=%" PRIu64 "; время=%.6g с\n",
                methods[index], settings->selected[index] ? "да" : "нет",
                settings->limits[index].max_iterations,
                settings->limits[index].max_seconds);
    }
    fprintf(stream, "Все 64-битные ключи — учебные, не для защиты данных.\n");
    fprintf(stream, "Проверка d выполняется по phi(n), а не по lambda(n).\n");
    if (data->count == 0) {
        fprintf(stream, "Входных записей нет.\n");
    }
    for (size_t index = 0; index < data->count; ++index) {
        const InputRecord *record = &data->records[index];
        fprintf(stream, "\nЗапись %zu, строка %zu\n", index + 1, record->line_number);
        if (record->kind == RECORD_ERROR) {
            fprintf(stream, "Ошибка ввода: %s\n", record->error);
        } else {
            fprintf(stream, "n=%s; e=%s\n", record->n, record->e);
            if (record->kind == RECORD_LARGE) {
                fprintf(stream, "Информационный режим: n — %u бит, e — %u бит.\n",
                        record->n_bits, record->e_bits);
                fprintf(stream, "Факторизация, общие множители и контроль p,q,d недоступны.\n");
            } else {
                write_key(stream, &data->batch.reports[record->report_index]);
            }
        }
    }
    for (size_t index = 0; index < data->batch.pair_count; ++index) {
        const CommonFactorRecord *pair = &data->batch.pairs[index];
        size_t first_line = 0, second_line = 0;
        for (size_t row = 0; row < data->count; ++row) {
            const InputRecord *record = &data->records[row];
            if (record->kind == RECORD_KEY && record->report_index == pair->first) {
                first_line = record->line_number;
            }
            if (record->kind == RECORD_KEY && record->report_index == pair->second) {
                second_line = record->line_number;
            }
        }
        const char *kind = "общий нетривиальный множитель";
        if (pair->kind == DUPLICATE_MODULUS) {
            kind = "совпадающие модули (не факторизация)";
        } else if (pair->kind == DIVISIBLE_MODULUS) {
            kind = "один модуль делит другой";
        }
        fprintf(stream, "\nСтроки %zu и %zu: %s; НОД=%" PRIu64 "\n",
                first_line, second_line, kind, pair->gcd);
    }
    bool success = fflush(stream) == 0 && !ferror(stream);
    return success;
}

void cleanup(AppData *data) {
    if (data != NULL) {
        free_batch_report(&data->batch);
        free(data->records);
        *data = (AppData){0};
    }
}
