#include "input_output.h"
#include "factorization.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

static void show_help(void) {
    puts("Учебная оценка RSA. Без аргументов — интерактивное меню.\n"
         "Запуск: rsa_analyzer --file файл [параметры]\n"
         "    или rsa_analyzer --key \"n e [p q d]\" [параметры]\n"
         "--compare              независимое сравнение методов\n"
         "--methods 0..7         маска: 1 — перебор, 2 — Ферма, 4 — Поллард\n"
         "--trial-bound число    граница D, не меньше 2\n"
         "--pollard-bound число  граница B, не меньше 2\n"
         "--base число           основание Полларда, не меньше 2\n"
         "--iterations число     лимит каждого метода, положительный\n"
         "--seconds число        лимит секунд каждого метода, точка в дроби\n"
         "--output файл          сохранить новый отчёт (без перезаписи)\n"
         "--help                 справка\n"
         "Файл UTF-8: одна запись n e или n e p q d в строке.\n"
         "Отсутствующие p,q,d: -. Комментарии начинаются с # после пробела.\n"
         "До 256 записей, до 1024 цифр в n и e.");
}

static bool apply_option(const char *option,
                         const char *value,
                         AnalysisSettings *settings) {
    uint64_t number = 0;
    bool valid = true;
    if (strcmp(option, "--seconds") == 0) {
        double seconds = 0;
        valid = parse_positive_seconds(value, &seconds);
        if (valid) {
            for (size_t index = 0; index < RSA_METHOD_COUNT; ++index) {
                settings->limits[index].max_seconds = seconds;
            }
        }
    } else {
        valid = parse_u64(value, &number);
        if (valid) {
            if (strcmp(option, "--methods") == 0) {
                valid = number <= 7;
                for (size_t index = 0; valid && index < RSA_METHOD_COUNT; ++index) {
                    settings->selected[index] = (number & (UINT64_C(1) << index)) != 0;
                }
            } else if (strcmp(option, "--trial-bound") == 0) {
                settings->trial_bound = number;
            } else if (strcmp(option, "--pollard-bound") == 0) {
                settings->pollard_bound = number;
            } else if (strcmp(option, "--base") == 0) {
                settings->pollard_base = number;
            } else if (strcmp(option, "--iterations") == 0) {
                for (size_t index = 0; index < RSA_METHOD_COUNT; ++index) {
                    settings->limits[index].max_iterations = number;
                }
            } else {
                valid = false;
            }
        }
    }
    return valid;
}

static int process_request(const char *file_path,
                           const char *key_text,
                           const char *output_path,
                           const AnalysisSettings *settings) {
    AppData data = {0};
    bool loaded = false;
    int code = 0;
    if (file_path != NULL) {
        FILE *stream = open_text(file_path, "rb");
        if (stream != NULL) {
            loaded = read_input(stream, &data);
            if (fclose(stream) != 0) {
                loaded = false;
            }
        }
    } else if (key_text != NULL && strlen(key_text) < MAX_LINE) {
        data.records = calloc(1, sizeof(*data.records));
        if (data.records != NULL) {
            char line[MAX_LINE];
            strcpy(line, key_text);
            data.count = 1;
            data.records[0].line_number = 1;
            validate_input(line, &data.records[0]);
            loaded = true;
        }
    }
    if (!loaded) {
        fputs("Ошибка чтения, выделения памяти или превышен предел 256 записей.\n", stderr);
        code = 2;
    } else {
        describe_large_keys(&data);
        AnalysisStatus status = analyze_input(&data, settings);
        if (status != OK) {
            fputs("Не удалось выполнить анализ: параметры или память.\n", stderr);
            code = 2;
        } else {
            for (size_t index = 0; index < data.count; ++index) {
                if (data.records[index].kind == RECORD_ERROR) {
                    code = 2;
                }
            }
            if (!write_report(stdout, &data, settings)) {
                code = 3;
            }
            if (output_path != NULL && output_path[0] != '\0') {
                /* Режим x защищает вход и существующие файлы от перезаписи. */
                FILE *output = open_text(output_path, "wx");
                if (output == NULL) {
                    fputs("Не удалось создать отчёт: файл существует или путь недоступен.\n", stderr);
                    code = 3;
                } else {
                    bool written = write_report(output, &data, settings);
                    if (fclose(output) != 0) {
                        written = false;
                    }
                    if (!written) {
                        fputs("Ошибка записи отчёта.\n", stderr);
                        code = 3;
                    }
                }
            }
        }
    }
    cleanup(&data);
    return code;
}

static bool prompt(const char *message,
                   char *buffer,
                   size_t size) {
    fputs(message, stdout);
    fflush(stdout);
    int status = read_line(stdin, buffer, size);
    if (status < 0) {
        puts("Ошибка: строка слишком длинная или недопустимая.");
    }
    return status == 1;
}

static void configure(AnalysisSettings *settings) {
    const char *options[] = {"--methods", "--trial-bound", "--pollard-bound",
                             "--base", "--iterations", "--seconds"};
    const char *prompts[] = {"Маска методов 0..7: ", "Граница D: ",
        "Граница B: ", "Основание Полларда: ",
        "Лимит итераций каждого метода: ", "Лимит секунд (например 1.0): "};
    AnalysisSettings candidate = *settings;
    char value[128];
    bool valid = true;
    puts("Пустая строка сохраняет текущее значение.");
    size_t index = 0;
    while (valid && index < 6) {
        valid = prompt(prompts[index], value, sizeof(value));
        if (valid && value[0] != '\0') {
            valid = apply_option(options[index], value, &candidate);
        }
        ++index;
    }
    if (valid) {
        valid = prompt("Режим: 0 — обычный, 1 — сравнение: ", value, sizeof(value));
        if (valid && value[0] != '\0') {
            valid = strcmp(value, "0") == 0 || strcmp(value, "1") == 0;
            candidate.compare_mode = strcmp(value, "1") == 0;
        }
    }
    if (valid && settings_valid(&candidate)) {
        *settings = candidate;
        puts("Настройки сохранены.");
    } else {
        puts("Ошибка настроек. Предыдущие значения сохранены.");
    }
}

static void interactive(void) {
    AnalysisSettings settings = default_settings();
    bool running = true;
    char command[32], input[MAX_LINE], output[MAX_LINE];
    while (running) {
        puts("\n1 — Ввести ключ; 2 — Прочитать файл; 3 — Настройки; 4 — Справка; 0 — Выход");
        if (!prompt("Выбор: ", command, sizeof(command))) {
            running = false;
        } else if (strcmp(command, "0") == 0) {
            running = false;
        } else if (strcmp(command, "3") == 0) {
            configure(&settings);
        } else if (strcmp(command, "4") == 0) {
            show_help();
        } else if (strcmp(command, "1") == 0 || strcmp(command, "2") == 0) {
            bool from_file = strcmp(command, "2") == 0;
            const char *message = from_file ? "Путь к файлу без кавычек: " :
                                               "Введите n e [p q d]: ";
            if (prompt(message, input, sizeof(input)) &&
                prompt("Новый файл отчёта (пусто — только экран): ", output, sizeof(output))) {
                (void)process_request(from_file ? input : NULL,
                                      from_file ? NULL : input, output, &settings);
            }
        } else {
            puts("Неизвестная команда.");
        }
    }
}

static int run_application(int argc,
                           char **argv) {
    AnalysisSettings settings = default_settings();
    const char *file = NULL, *key = NULL, *output = NULL;
    bool valid = true, help = false;
    int index = 1, code = 0;
    while (valid && index < argc) {
        const char *option = argv[index];
        if (strcmp(option, "--help") == 0) {
            help = true;
        } else if (strcmp(option, "--compare") == 0) {
            settings.compare_mode = true;
        } else if (index + 1 >= argc) {
            valid = false;
        } else {
            const char *value = argv[++index];
            if (strcmp(option, "--file") == 0 && file == NULL) {
                file = value;
            } else if (strcmp(option, "--key") == 0 && key == NULL) {
                key = value;
            } else if (strcmp(option, "--output") == 0 && output == NULL) {
                output = value;
            } else {
                valid = apply_option(option, value, &settings);
            }
        }
        ++index;
    }
    if (argc == 1) {
        interactive();
    } else if (help) {
        show_help();
    } else if (!valid || !settings_valid(&settings) ||
               ((file == NULL) == (key == NULL))) {
        fputs("Недопустимые параметры. Используйте --help.\n", stderr);
        code = 2;
    } else {
        code = process_request(file, key, output, &settings);
    }
    return code;
}

int main(int argc,
         char **argv) {
    int code = 0;
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    /* Преобразуем Unicode-аргументы Windows в UTF-8, включая пути. */
    int wide_count = 0;
    LPWSTR *wide_args = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    char **utf_args = NULL;
    bool ready = wide_args != NULL && wide_count > 0;
    if (ready) {
        utf_args = calloc((size_t)wide_count, sizeof(*utf_args));
        ready = utf_args != NULL;
    }
    for (int index = 0; ready && index < wide_count; ++index) {
        int length = WideCharToMultiByte(CP_UTF8, 0, wide_args[index], -1,
                                         NULL, 0, NULL, NULL);
        ready = length > 0;
        if (ready) {
            utf_args[index] = malloc((size_t)length);
            ready = utf_args[index] != NULL;
        }
        if (ready) {
            ready = WideCharToMultiByte(CP_UTF8, 0, wide_args[index], -1,
                                        utf_args[index], length, NULL, NULL) > 0;
        }
    }
    if (ready) {
        code = run_application(wide_count, utf_args);
    } else {
        fputs("Ошибка преобразования аргументов.\n", stderr);
        code = 2;
    }
    if (utf_args != NULL) {
        for (int index = 0; index < wide_count; ++index) {
            free(utf_args[index]);
        }
    }
    free(utf_args);
    if (wide_args != NULL) {
        LocalFree(wide_args);
    }
    (void)argc;
    (void)argv;
#else
    code = run_application(argc, argv);
#endif
    return code;
}
