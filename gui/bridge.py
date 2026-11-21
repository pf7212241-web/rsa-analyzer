"""Структурированный доступ к C; математические проверки выполняет DLL."""
import ctypes as c
import math
from pathlib import Path
import sys

U = c.c_uint64
Z = c.c_size_t
B = c.c_bool
I = c.c_int


class Key(c.Structure):
    _fields_ = [('n', U), ('e', U)]


class Reference(c.Structure):
    _fields_ = [('p', U), ('q', U), ('d', U), ('has_p', B), ('has_q', B), ('has_d', B)]


class Limits(c.Structure):
    _fields_ = [('max_iterations', U), ('max_seconds', c.c_double)]


class Settings(c.Structure):
    _fields_ = [('trial_bound', U), ('pollard_bound', U), ('pollard_base', U),
                ('selected', B * 3), ('limits', Limits * 3), ('compare_mode', B)]


class Factor(c.Structure):
    _fields_ = [('method', I), ('status', I), ('reason', I), ('u', U), ('v', U),
                ('iterations', U), ('elapsed', c.c_double)]


class Pair(c.Structure):
    _fields_ = [('first', Z), ('second', Z), ('gcd', U), ('kind', I)]


class Report(c.Structure):
    _fields_ = [('key', Key), ('bit_length', c.c_uint), ('methods', Factor * 3),
                ('has_factors', B), ('prime_pair', B), ('u', U), ('v', U),
                ('model_status', I), ('control_status', I)]


class Batch(c.Structure):
    _fields_ = [('reports', c.POINTER(Report)), ('count', Z),
                ('pairs', c.POINTER(Pair)), ('pair_count', Z)]


class Record(c.Structure):
    _fields_ = [('line_number', Z), ('report_index', Z), ('kind', I), ('key', Key),
                ('ref', Reference), ('n', c.c_char * 1025), ('e', c.c_char * 1025),
                ('error', c.c_char * 256), ('n_bits', c.c_uint), ('e_bits', c.c_uint)]


class Data(c.Structure):
    _fields_ = [('records', c.POINTER(Record)), ('count', Z), ('batch', Batch)]


DEFAULTS = dict(trial_bound='100000', pollard_bound='10000', pollard_base='2',
                iterations='100000', seconds='1.0', selected=[True, True, True],
                compare=False)
METHODS = ('Пробное деление', 'Ферма', 'Поллард p−1')
STATUSES = ('Разложение найдено', 'Поиск завершён', 'Достигнут лимит',
            'Вырожденный результат', 'Ошибка параметров', 'Операция недоступна', 'Не запускался')
REASONS = ('подтверждены делители', 'полный поиск', 'достигнута граница метода',
           'лимит итераций', 'лимит времени', 'граница арифметики',
           'недопустимые параметры', 'основание не меньше n', 'НОД равен n',
           'не выбран', 'разложение уже известно', 'ошибка часов')
MODELS = ('не установлено', 'соответствует', 'не соответствует')
CONTROLS = ('не заданы', 'проверка отложена до факторизации',
            'подтверждены в учебной модели', 'не согласованы с учебной моделью')


def resource_root():
    return Path(sys._MEIPASS) if getattr(sys, 'frozen', False) else Path(__file__).resolve().parents[1]


def validate_settings(values):
    errors = {}
    for name in ('trial_bound', 'pollard_bound', 'pollard_base', 'iterations'):
        text = str(values.get(name, ''))
        minimum = 1 if name == 'iterations' else 2
        if (not text.isascii() or not text.isdecimal() or len(text) > 20
                or not minimum <= int(text) <= 2**64 - 1):
            errors[name] = f'Целое число от {minimum} до 18446744073709551615.'
    try:
        seconds = float(values.get('seconds', ''))
        if not math.isfinite(seconds) or seconds <= 0:
            raise ValueError
    except (ValueError, TypeError):
        errors['seconds'] = 'Положительное число секунд, например 1.0.'
    return errors


def make_settings(values):
    if validate_settings(values):
        raise ValueError('Исправьте настройки анализа.')
    result = Settings()
    for name in ('trial_bound', 'pollard_bound', 'pollard_base'):
        setattr(result, name, int(values[name]))
    for index in range(3):
        result.selected[index] = bool(values['selected'][index])
        result.limits[index] = Limits(int(values['iterations']), float(values['seconds']))
    result.compare_mode = bool(values['compare'])
    return result


def library():
    root = resource_root()
    path = root / 'rsa_core.dll' if getattr(sys, 'frozen', False) else root / 'build/rsa_core.dll'
    try:
        lib = c.CDLL(str(path.resolve()))
    except OSError as error:
        raise RuntimeError('Не удалось загрузить библиотеку C. Запускайте exe из полной папки поставки '
                           'вместе с каталогом _internal; для исходников сначала выполните сборку DLL.') from error
    signatures = {
        'gui_layout': ([Z], Z),
        'gui_load': ([c.c_char_p, I, c.POINTER(Data)], I),
        'describe_large_keys': ([c.POINTER(Data)], None),
        'analyze_input': ([c.POINTER(Data), c.POINTER(Settings)], I),
        'gui_write_report': ([c.c_char_p, c.POINTER(Data), c.POINTER(Settings)], I),
        'cleanup': ([c.POINTER(Data)], None),
    }
    for name, (args, result) in signatures.items():
        fn = getattr(lib, name)
        fn.argtypes, fn.restype = args, result
    expected = []
    for structure in (Key, Reference, Limits, Settings, Factor, Pair, Report, Batch, Record, Data):
        expected.append(c.sizeof(structure))
        expected.extend(getattr(structure, field).offset for field, _ in structure._fields_)
    if any(lib.gui_layout(index) != value for index, value in enumerate(expected)):
        raise RuntimeError('Версии интерфейса и библиотеки несовместимы. Используйте полную папку поставки.')
    return lib


def scalar_fields(value):
    return {name: getattr(value, name) for name, _ in value._fields_}


def analyze(request, report_path):
    lib = library()
    settings = make_settings(request['settings'])
    data = Data()
    source = request['source']
    if '\0' in source:
        raise ValueError('Ввод содержит нулевой символ.')
    try:
        if lib.gui_load(source.encode('utf-8'), request['mode'] == 'file', c.byref(data)):
            raise ValueError('Не удалось прочитать данные. Проверьте путь, доступ к файлу и предел 256 записей.')
        lib.describe_large_keys(c.byref(data))
        if lib.analyze_input(c.byref(data), c.byref(settings)):
            raise RuntimeError('Не удалось выполнить анализ: настройки или выделение памяти.')
        rows = []
        for index in range(data.count):
            record = data.records[index]
            row = dict(line=record.line_number, kind=record.kind, n=record.n.decode('utf-8'),
                       e=record.e.decode('utf-8'), error=record.error.decode('utf-8'),
                       n_bits=record.n_bits, e_bits=record.e_bits, reference={}, methods=[])
            row['reference'] = {name: getattr(record.ref, name) if getattr(record.ref, 'has_' + name) else None
                                for name in ('p', 'q', 'd')}
            if record.kind == 0:
                report = data.batch.reports[record.report_index]
                row.update({name: getattr(report, name) for name in
                            ('bit_length', 'has_factors', 'prime_pair', 'u', 'v', 'model_status', 'control_status')})
                row['methods'] = [scalar_fields(item) for item in report.methods]
                row['report_index'] = record.report_index
            rows.append(row)
        pairs = [scalar_fields(data.batch.pairs[index]) for index in range(data.batch.pair_count)]
        line_map = {r['report_index']: r['line'] for r in rows if r['kind'] == 0}
        for pair in pairs:
            pair['first_line'], pair['second_line'] = line_map[pair['first']], line_map[pair['second']]
        if lib.gui_write_report(str(report_path).encode('utf-8'), c.byref(data), c.byref(settings)):
            raise OSError('Не удалось сформировать текст отчёта.')
        report_text = Path(report_path).read_text(encoding='utf-8')
        # Добавляем контрольный ввод, которого нет в старом консольном отчёте.
        controls = ['\nКОНТРОЛЬНЫЕ ДАННЫЕ ЗАПУСКА']
        for row in rows:
            if any(value is not None for value in row['reference'].values()):
                controls.append(f"Строка {row['line']}: " + ', '.join(
                    f'{name}={value if value is not None else "-"}' for name, value in row['reference'].items()))
        if len(controls) > 1:
            report_text += '\n'.join(controls) + '\n'
        return dict(rows=rows, pairs=pairs, report=report_text, request=request)
    finally:
        lib.cleanup(c.byref(data))
