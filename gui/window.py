"""Графическая оболочка. Расчёты и файловый разбор выполняются в C."""
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

from bridge import (DEFAULTS, METHODS, STATUSES, REASONS, MODELS, CONTROLS,
                    resource_root, validate_settings)
from jobs import AnalysisJob

BG, WHITE, INK, MUTED, BLUE, LINE = '#F4F6F9', '#FFFFFF', '#192B40', '#56677C', '#2459A6', '#D7DEE8'


class ScrollPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.canvas = tk.Canvas(self, bg=BG, highlightthickness=0)
        bar = ttk.Scrollbar(self, orient='vertical', command=self.canvas.yview)
        self.canvas.configure(yscrollcommand=bar.set)
        self.canvas.pack(side='left', fill='both', expand=True)
        bar.pack(side='right', fill='y')
        self.body = ttk.Frame(self.canvas, padding=18)
        self.item = self.canvas.create_window((0, 0), anchor='nw', window=self.body)
        self.body.bind('<Configure>', lambda event: self.canvas.configure(scrollregion=self.canvas.bbox('all')))
        self.canvas.bind('<Configure>', lambda event: self.canvas.itemconfigure(self.item, width=event.width))


def text_box(parent, height=8):
    frame = ttk.Frame(parent)
    text = tk.Text(frame, height=height, wrap='word', bg=WHITE, fg=INK,
                   font=('Segoe UI', 10), relief='flat', padx=12, pady=9,
                   highlightthickness=1, highlightbackground=LINE,
                   highlightcolor=BLUE, insertbackground=INK, state='disabled')
    scroll = ttk.Scrollbar(frame, orient='vertical', command=text.yview)
    text.configure(yscrollcommand=scroll.set)
    text.pack(side='left', fill='both', expand=True)
    scroll.pack(side='right', fill='y')
    return frame, text


def put_text(widget, value):
    widget.configure(state='normal')
    widget.delete('1.0', 'end')
    widget.insert('1.0', value)
    widget.configure(state='disabled')


class Application(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title('RSA — оценка ключей')
        self.geometry('1140x800')
        self.minsize(900, 650)
        self.configure(bg=BG)
        self.job = None
        self.poll_token = None
        self.result = None
        self.result_signature = None
        self.running_signature = None
        self._batch_change = False
        self.protocol('WM_DELETE_WINDOW', self.close)
        self._styles()
        self.values = {name: tk.StringVar(value='') for name in ('n', 'e', 'p', 'q', 'd', 'file')}
        self.mode = tk.StringVar(value='key')
        self.controls = tk.BooleanVar(value=False)
        self.settings = {name: tk.StringVar(value=DEFAULTS[name]) for name in
                         ('trial_bound', 'pollard_bound', 'pollard_base', 'iterations', 'seconds')}
        self.selected = [tk.BooleanVar(value=True) for _ in METHODS]
        self.compare = tk.BooleanVar(value=False)
        self.status = tk.StringVar(value='Введите открытый ключ или выберите файл.')
        self.form_error = tk.StringVar()
        self.summary = tk.StringVar(value='Результаты появятся после анализа.')
        self.stamp = tk.StringVar(value='Завершённых запусков пока нет.')
        self._header()
        self.tabs = ttk.Notebook(self)
        self.tabs.pack(fill='both', expand=True, padx=18, pady=(0, 10))
        self.analysis_page, self.settings_page, self.help_page = [ScrollPage(self.tabs) for _ in range(3)]
        for page, name in zip((self.analysis_page, self.settings_page, self.help_page),
                              ('Анализ', 'Настройки', 'Инструкция')):
            self.tabs.add(page, text=name, padding=0)
        self._analysis(self.analysis_page.body)
        self._settings(self.settings_page.body)
        self._help(self.help_page.body)
        footer = ttk.Frame(self, padding=(22, 0, 22, 12))
        footer.pack(fill='x')
        self.save_button = ttk.Button(footer, text='Сохранить отчёт…', command=self.save, state='disabled')
        self.save_button.pack(side='right', padx=(14, 0))
        self.status_label = ttk.Label(footer, textvariable=self.status, style='Muted.TLabel', wraplength=720)
        self.status_label.pack(side='left', fill='x', expand=True)
        footer.bind('<Configure>', lambda event: self.status_label.configure(wraplength=max(300, event.width - 225)))
        icon_path = resource_root() / 'assets/rsa.png'
        if icon_path.exists():
            self.app_icon = tk.PhotoImage(file=str(icon_path))
            self.iconphoto(True, self.app_icon)
        for variable in [*self.values.values(), *self.settings.values(), *self.selected, self.compare, self.mode]:
            variable.trace_add('write', self.changed)
        self.bind_all('<Control-a>', self.select_all, add='+')
        self.bind_all('<Control-A>', self.select_all, add='+')
        self.bind_all('<MouseWheel>', self.scroll_page, add='+')
        self._source_mode()

    def _styles(self):
        style = ttk.Style(self)
        style.theme_use('clam')
        style.configure('.', background=BG, foreground=INK, font=('Segoe UI', 10))
        style.configure('TFrame', background=BG)
        style.configure('TLabel', background=BG, foreground=INK)
        style.configure('Muted.TLabel', foreground=MUTED)
        style.configure('Error.TLabel', foreground='#A33131')
        style.configure('Title.TLabel', font=('Segoe UI Semibold', 24))
        style.configure('Heading.TLabel', font=('Segoe UI Semibold', 13))
        style.configure('TButton', padding=(12, 8), background=WHITE, bordercolor=LINE)
        style.map('TButton', background=[('active', '#E7EDF6')])
        style.configure('Primary.TButton', background=BLUE, foreground=WHITE, bordercolor=BLUE)
        style.map('Primary.TButton', background=[('disabled', '#A0B2CA'), ('active', '#194782')],
                  foreground=[('disabled', '#FFFFFF')])
        style.configure('TEntry', fieldbackground=WHITE, padding=7, bordercolor=LINE)
        style.map('TEntry', bordercolor=[('focus', BLUE)])
        style.configure('TNotebook', background=BG, borderwidth=0)
        style.configure('TNotebook.Tab', padding=(24, 11), background=BG, foreground=MUTED)
        style.map('TNotebook.Tab', background=[('selected', WHITE)], foreground=[('selected', BLUE)])
        style.configure('Treeview', background=WHITE, fieldbackground=WHITE, rowheight=29,
                        bordercolor=LINE, font=('Segoe UI', 10))
        style.configure('Treeview.Heading', background='#E9EEF5', foreground=INK, padding=6)
        style.configure('Horizontal.TProgressbar', background=BLUE, troughcolor=LINE,
                        bordercolor=BG, lightcolor=BLUE, darkcolor=BLUE)
        style.map('Treeview', background=[('selected', '#DDE8F8')], foreground=[('selected', INK)])

    def _header(self):
        header = ttk.Frame(self, padding=(24, 17, 24, 17))
        header.pack(fill='x')
        ttk.Label(header, text='RSA — оценка ключей', style='Title.TLabel').pack(anchor='w')
        ttk.Label(header, text='Учебный анализ стойкости', style='Muted.TLabel').pack(anchor='w', pady=(2, 0))
        formula = tk.Label(header, text='n = p × q', font=('Consolas', 23), fg=BLUE, bg=BG)
        formula.place(relx=1, rely=0.5, anchor='e')

    def _analysis(self, parent):
        parent.columnconfigure(1, weight=1)
        left = ttk.Frame(parent, padding=(0, 0, 22, 0))
        left.grid(row=0, column=0, sticky='new')
        right = ttk.Frame(parent)
        right.grid(row=0, column=1, sticky='nsew')
        ttk.Label(left, text='Исходные данные', style='Heading.TLabel').pack(anchor='w', pady=(0, 10))
        modes = ttk.Frame(left)
        modes.pack(fill='x', pady=(0, 12))
        for name, value in [('Один ключ', 'key'), ('Из файла', 'file')]:
            ttk.Radiobutton(modes, text=name, variable=self.mode, value=value,
                            command=self._source_mode).pack(side='left', padx=(0, 10))
        self.key_fields = ttk.Frame(left)
        self.key_fields.pack(fill='x')
        for key, label in [('n', 'Модуль n'), ('e', 'Открытая экспонента e')]:
            self._entry(self.key_fields, label, self.values[key])
        ttk.Checkbutton(self.key_fields, text='Показать контрольные p, q, d', variable=self.controls,
                        command=self._toggle_controls).pack(anchor='w', pady=(6, 8))
        self.reference_fields = ttk.Frame(self.key_fields)
        for key, label in [('p', 'Множитель p'), ('q', 'Множитель q'), ('d', 'Закрытая экспонента d')]:
            self._entry(self.reference_fields, label, self.values[key])
        self.file_fields = ttk.Frame(left)
        ttk.Label(self.file_fields, text='Файл UTF-8 с ключами').pack(anchor='w')
        ttk.Entry(self.file_fields, textvariable=self.values['file'], width=29).pack(fill='x', pady=6)
        ttk.Button(self.file_fields, text='Выбрать файл…', command=self.choose_file).pack(fill='x')
        ttk.Label(self.file_fields, text='Одна строка: n e или n e p q d\nДо 256 записей в наборе.',
                  style='Muted.TLabel', wraplength=280).pack(anchor='w', pady=9)
        self.input_tail = ttk.Frame(left)
        self.input_tail.pack(fill='x')
        ttk.Label(self.input_tail, textvariable=self.form_error, style='Error.TLabel',
                  wraplength=280).pack(fill='x', pady=(2, 8))
        self.run_button = ttk.Button(self.input_tail, text='Выполнить анализ', style='Primary.TButton', command=self.start)
        self.run_button.pack(fill='x')
        self.stop_button = ttk.Button(self.input_tail, text='Остановить', command=self.stop, state='disabled')
        self.stop_button.pack(fill='x', pady=(6, 0))
        self.progress = ttk.Progressbar(self.input_tail, mode='indeterminate')
        self.progress.pack(fill='x', pady=10)
        ttk.Button(self.input_tail, text='Загрузить пример Ферма', command=lambda: self.example('fermat')).pack(fill='x')
        ttk.Button(self.input_tail, text='Методы и лимиты…', command=lambda: self.tabs.select(self.settings_page)).pack(fill='x', pady=6)
        ttk.Label(self.input_tail, text='До 64 бит — вычислительный анализ.\nБольшие числа — только информация.',
                  wraplength=280, style='Muted.TLabel').pack(anchor='w', pady=(10, 0))
        ttk.Label(right, text='Результаты анализа', style='Heading.TLabel').pack(anchor='w')
        ttk.Label(right, textvariable=self.stamp, style='Muted.TLabel', wraplength=600).pack(anchor='w', pady=(4, 10))
        self.records = self._tree(right, ('line', 'n', 'status'), ('Строка', 'Модуль n', 'Результат'), (65, 180, 240), 3)
        self.records.bind('<<TreeviewSelect>>', self.show_record)
        self.summary_label = ttk.Label(right, textvariable=self.summary, wraplength=600, font=('Segoe UI Semibold', 11))
        self.summary_label.pack(fill='x', pady=(14, 8))
        right.bind('<Configure>', lambda event: self.summary_label.configure(wraplength=max(250, event.width - 10)))
        self.methods = self._tree(right, ('name', 'status', 'iterations', 'time'),
                                  ('Метод', 'Результат', 'Итерации', 'Время, с'), (115, 190, 90, 90), 3)
        ttk.Label(right, text='Подробности и пояснения', style='Muted.TLabel').pack(anchor='w', pady=(12, 4))
        details_frame, self.details = text_box(right, height=6)
        details_frame.pack(fill='both', expand=True)
        put_text(self.details, 'Введите n и e слева или откройте пример из инструкции.\n\n'
                 'Отсутствие найденного разложения не является доказательством стойкости ключа.')

    @staticmethod
    def _entry(parent, label, variable):
        ttk.Label(parent, text=label).pack(anchor='w')
        field = ttk.Entry(parent, textvariable=variable, width=29)
        field.pack(fill='x', pady=(4, 9))
        return field

    @staticmethod
    def _tree(parent, columns, headings, widths, height):
        frame = ttk.Frame(parent)
        frame.pack(fill='x')
        tree = ttk.Treeview(frame, columns=columns, show='headings', height=height, selectmode='browse')
        for column, title, width in zip(columns, headings, widths):
            tree.heading(column, text=title)
            tree.column(column, width=width, minwidth=55, stretch=column != 'line')
        vertical = ttk.Scrollbar(frame, orient='vertical', command=tree.yview)
        horizontal = ttk.Scrollbar(frame, orient='horizontal', command=tree.xview)
        tree.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
        tree.grid(row=0, column=0, sticky='nsew')
        vertical.grid(row=0, column=1, sticky='ns')
        horizontal.grid(row=1, column=0, sticky='ew')
        frame.columnconfigure(0, weight=1)
        return tree

    def _settings(self, parent):
        ttk.Label(parent, text='Параметры поиска', style='Heading.TLabel').pack(anchor='w')
        ttk.Label(parent, text='Настройки применяются к следующему запуску. Лимиты задаются для каждого метода.',
                  style='Muted.TLabel', wraplength=750).pack(anchor='w', pady=(6, 15))
        methods = ttk.Frame(parent)
        methods.pack(anchor='w')
        for name, variable in zip(METHODS, self.selected):
            ttk.Checkbutton(methods, text=name, variable=variable).pack(side='left', padx=(0, 24))
        ttk.Checkbutton(parent, text='Независимое сравнение методов', variable=self.compare).pack(anchor='w', pady=(14, 5))
        ttk.Label(parent, text='В обычном режиме дальнейший поиск может быть пропущен, если множители уже известны.',
                  style='Muted.TLabel', wraplength=750).pack(anchor='w', pady=(0, 18))
        grid = ttk.Frame(parent)
        grid.pack(fill='x')
        grid.columnconfigure(1, weight=1)
        self.setting_errors = {}
        labels = [('trial_bound', 'Граница пробного деления D', 'Максимальный проверяемый делитель.'),
                  ('pollard_bound', 'Граница Полларда B', 'Последний показатель j в цепочке степеней.'),
                  ('pollard_base', 'Основание Полларда', 'Для нечётного n: не меньше 2 и меньше n.'),
                  ('iterations', 'Лимит итераций', 'Одинаковый лимит для каждого выбранного метода.'),
                  ('seconds', 'Лимит времени, с', 'Например 1.0. Проверяется между итерациями.')]
        for row, (name, label, hint) in enumerate(labels):
            cell = ttk.Frame(grid, padding=(0, 0, 18, 10))
            cell.grid(row=row, column=0, sticky='nw')
            ttk.Label(cell, text=label).pack(anchor='w')
            ttk.Entry(cell, textvariable=self.settings[name], width=25).pack(anchor='w', pady=4)
            explanation = ttk.Frame(grid)
            explanation.grid(row=row, column=1, sticky='nw', pady=(20, 0))
            ttk.Label(explanation, text=hint, style='Muted.TLabel', wraplength=480).pack(anchor='w')
            self.setting_errors[name] = tk.StringVar()
            ttk.Label(explanation, textvariable=self.setting_errors[name], style='Error.TLabel', wraplength=480).pack(anchor='w')
        ttk.Button(parent, text='Восстановить значения по умолчанию', command=self.defaults).pack(anchor='w', pady=12)
        ttk.Label(parent, text='Счётчики разных методов отражают разные операции. '
                  'Достижение лимита не подтверждает стойкость ключа.', style='Muted.TLabel', wraplength=780).pack(anchor='w')

    def _help(self, parent):
        ttk.Label(parent, text='Как пользоваться программой', style='Heading.TLabel').pack(anchor='w')
        ttk.Label(parent, text='Исследуйте ключ, сравните методы и сохраните полученный результат.',
                  style='Muted.TLabel').pack(anchor='w', pady=(5, 14))
        example = tk.Canvas(parent, height=155, bg=WHITE, highlightthickness=1, highlightbackground=LINE)
        example.pack(fill='x')
        example.create_text(22, 23, text='Пример метода Ферма', anchor='w', fill=INK, font=('Segoe UI Semibold', 12))
        example.create_text(22, 63, text='10403  =  102² − 1²  =  101 × 103', anchor='w', fill=BLUE, font=('Consolas', 21))
        example.create_text(22, 106, text='n = 10403, e = 7. Начинаем с a = 102: a² − n = 1.',
                            anchor='w', fill=INK, font=('Segoe UI', 11))
        example.create_text(22, 132, text='Множители: a − 1 = 101 и a + 1 = 103.',
                            anchor='w', fill=MUTED, font=('Segoe UI', 11))
        actions = ttk.Frame(parent)
        actions.pack(fill='x', pady=12)
        ttk.Button(actions, text='Подставить пример Ферма', style='Primary.TButton',
                    command=lambda: self.example('fermat')).pack(side='left', padx=(0, 10))
        ttk.Button(actions, text='Пример Полларда p−1', command=lambda: self.example('pollard')).pack(side='left', padx=(0, 10))
        ttk.Button(actions, text='Набор ключей', command=lambda: self.example('file')).pack(side='left')
        sections = [
            ('1. Введите данные', 'На вкладке «Анализ» укажите модуль n и открытую экспоненту e. '
             'Используйте десятичные цифры без знака. Для вычислений требуется n > 1 и 1 < e < n. '
             'Контрольные p, q, d необязательны: оставьте неизвестные значения пустыми.'),
            ('2. Выберите методы', 'На вкладке «Настройки» доступны пробное деление, Ферма и Поллард p−1. '
             'Включите независимое сравнение, чтобы каждый выбранный метод работал с исходным n. '
             'В обычном режиме известное разложение позволяет пропустить последующий поиск. '
             'Если отключить все три метода, диагностика и поиск общих множителей в наборе сохраняются.'),
            ('3. Запустите анализ', 'Нажмите «Выполнить анализ». Окно остаётся доступным. '
             '«Остановить» отменяет текущий запрос; неполный отчёт не сохраняется. '
             'Предыдущий завершённый результат остаётся видимым. Лимиты времени относятся к отдельным методам, '
             'а не ко всему запросу.'),
            ('4. Прочитайте результат', 'Выберите строку набора, чтобы увидеть методы и подробности. '
             '«Разложение найдено» означает подтверждённые делители. Соответствие учебной модели проверяется отдельно: '
             'n — произведение двух различных нечётных простых, e взаимно просто с φ(n). '
             'Проверка d использует e·d ≡ 1 mod φ(n). Малое e само по себе не объявляется уязвимостью. '
             'Если факторизация не найдена, это не доказывает стойкость.'),
            ('Файловый ввод', 'Файл UTF-8: одна запись n e либо n e p q d на строку. '
             'Для неизвестных контрольных параметров используйте дефис. Пример: 3233 17 61 53 2753. '
             'Пустые строки и комментарии с # разрешены; перед # после данных нужен пробел. '
             'До 256 записей, до 1024 цифр в n и e. Ошибка отдельной строки не мешает анализу остальных.'),
            ('Большие числа и ограничения', 'Вычисления выполняются в диапазоне uint64_t, до 18446744073709551615. '
             'Если n или e больше, программа показывает разрядность и проверяет запись числа. '
             'Факторизация, общие множители и соотношения RSA в этом режиме не проверяются. '
             'Все 64-битные ключи здесь учебные и не предназначены для защиты данных.'),
            ('Сохранение и повторный запуск', '«Сохранить отчёт…» записывает данные и настройки завершённого запуска в UTF-8. '
             'Выберите новое имя файла: существующие файлы не перезаписываются. '
             'Изменение полей не меняет уже полученный отчёт — для новых данных запустите анализ снова.'),
            ('Другие примеры', 'Для Полларда используется n = 261883, e = 3: 261883 = 257 × 1019, '
             'причём 257 − 1 = 256. В наборе модулей 3233 и 3599 есть общий множитель 61. '
             'Кнопки примеров восстанавливают настройки и включают сравнение; запуск выполняется отдельно.'),
        ]
        for title, content in sections:
            ttk.Label(parent, text=title, style='Heading.TLabel').pack(anchor='w', pady=(16, 5))
            label = ttk.Label(parent, text=content, wraplength=920, justify='left')
            label.pack(fill='x', anchor='w')
            label.bind('<Configure>', lambda event, item=label: item.configure(wraplength=max(300, event.width - 12)))
        ttk.Separator(parent).pack(fill='x', pady=20)
        ttk.Label(parent, text='Курсовая работа «Создание программы для оценки стойкости ключей RSA»\n'
                  'Автор: Филиппов Павел. Версия интерфейса 1.0.', style='Muted.TLabel').pack(anchor='w')

    def _source_mode(self):
        self.key_fields.pack_forget()
        self.file_fields.pack_forget()
        frame = self.key_fields if self.mode.get() == 'key' else self.file_fields
        frame.pack(fill='x', before=self.input_tail)

    def _toggle_controls(self):
        if self.controls.get():
            self.reference_fields.pack(fill='x')
        else:
            self.reference_fields.pack_forget()

    def choose_file(self):
        path = filedialog.askopenfilename(title='Выберите файл с ключами',
                                          filetypes=[('Текстовые файлы', '*.txt'), ('Все файлы', '*.*')])
        if path:
            self.values['file'].set(path)

    def settings_values(self):
        return dict(**{name: variable.get().strip() for name, variable in self.settings.items()},
                    selected=[v.get() for v in self.selected], compare=self.compare.get())

    def signature(self):
        return (self.mode.get(), tuple(v.get() for v in self.values.values()),
                tuple(v.get() for v in self.settings.values()), tuple(v.get() for v in self.selected), self.compare.get())

    def changed(self, *_):
        if not self._batch_change and self.result:
            self._stamp()

    def _stamp(self):
        text = f"Завершено: {self.result['completed'].replace('T', ' ')}"
        if self.result_signature != self.signature():
            text += ' • Показан предыдущий запуск; данные или настройки изменены.'
        self.stamp.set(text)

    def defaults(self):
        for name, variable in self.settings.items():
            variable.set(DEFAULTS[name])
        for variable in self.selected:
            variable.set(True)
        self.compare.set(False)
        for variable in self.setting_errors.values():
            variable.set('')

    def example(self, kind):
        if self.job:
            messagebox.showinfo('Выполняется анализ', 'Сначала дождитесь результата или остановите расчёт.')
            return
        if any(variable.get() for variable in self.values.values()):
            if not messagebox.askyesno('Подставить пример', 'Заменить введённые данные и восстановить настройки для примера?'):
                return
        self._batch_change = True
        try:
            for variable in self.values.values():
                variable.set('')
            self.defaults()
            self.compare.set(True)
            self.controls.set(False)
            self._toggle_controls()
            self.mode.set('file' if kind == 'file' else 'key')
            if kind == 'file':
                self.values['file'].set(str(resource_root() / 'examples/keys.txt'))
            else:
                self.values['n'].set('10403' if kind == 'fermat' else '261883')
                self.values['e'].set('7' if kind == 'fermat' else '3')
            self._source_mode()
            self.form_error.set('')
            self.tabs.select(self.analysis_page)
            self.status.set('Пример подставлен. Нажмите «Выполнить анализ».')
        finally:
            self._batch_change = False
            self.changed()

    def request(self):
        settings = self.settings_values()
        errors = validate_settings(settings)
        for name, variable in self.setting_errors.items():
            variable.set(errors.get(name, ''))
        if errors:
            self.tabs.select(self.settings_page)
            raise ValueError('Исправьте отмеченные настройки.')
        if self.mode.get() == 'file':
            source = self.values['file'].get().strip()
            if not source:
                raise ValueError('Выберите файл с ключами.')
        else:
            fields = {name: self.values[name].get().strip() for name in ('n', 'e', 'p', 'q', 'd')}
            for name, value in fields.items():
                if not value and name in ('p', 'q', 'd'):
                    continue
                if not value or not value.isascii() or not value.isdecimal() or len(value) > 1024:
                    raise ValueError(f'{name}: введите от 1 до 1024 десятичных цифр без знака.')
                if name in ('p', 'q', 'd') and int(value) > 2**64 - 1:
                    raise ValueError(f'{name}: контрольное значение должно помещаться в 64 бита.')
            n, e = int(fields['n']), int(fields['e'])
            if not n or not e:
                raise ValueError('n и e должны быть положительными.')
            if n <= 2**64 - 1 and e <= 2**64 - 1 and not (n > 1 and 1 < e < n):
                raise ValueError('Для вычислительного анализа нужны n > 1 и 1 < e < n.')
            source = ' '.join(fields[name] or '-' for name in ('n', 'e', 'p', 'q', 'd'))
        return dict(mode=self.mode.get(), source=source, settings=settings)

    def start(self):
        if self.job:
            return
        try:
            request = self.request()
            self.running_signature = self.signature()
            self.job = AnalysisJob(request)
        except (ValueError, OSError) as error:
            self.form_error.set(str(error))
            self.status.set(str(error))
            return
        self.form_error.set('')
        self.run_button.configure(state='disabled')
        self.stop_button.configure(state='normal')
        self.progress.start(12)
        self.status.set('Выполняется анализ. Можно остановить текущий запрос.')
        if self.result:
            self.stamp.set('Идёт новый расчёт; ниже показан предыдущий завершённый запуск.')
        self.poll_token = self.after(100, self.poll)

    def poll(self):
        self.poll_token = None
        if not self.job:
            return
        try:
            payload = self.job.poll()
        except Exception as error:
            self.job.close()
            payload = dict(ok=False, error=str(error))
        if payload is None:
            self.poll_token = self.after(100, self.poll)
            return
        self.job = None
        self.progress.stop()
        self.run_button.configure(state='normal')
        self.stop_button.configure(state='disabled')
        if payload.get('cancelled'):
            self.status.set('Анализ остановлен. Неполный результат не сохранён.')
            if self.result:
                self._stamp()
        elif not payload['ok']:
            self.form_error.set(payload['error'])
            self.status.set('Не удалось завершить анализ. Исправьте причину и повторите запуск.')
            if self.result:
                self._stamp()
        else:
            self.result = payload['result']
            self.result_signature = self.running_signature
            self._stamp()
            self.records.delete(*self.records.get_children())
            self.methods.delete(*self.methods.get_children())
            for index, row in enumerate(self.result['rows']):
                verdict = ('Ошибка ввода' if row['kind'] == 2 else 'Информационный режим' if row['kind'] == 1 else
                           'Разложение найдено' if row['has_factors'] else 'Разложение не найдено')
                self.records.insert('', 'end', iid=str(index), values=(row['line'], row['n'], verdict))
            if self.result['rows']:
                self.records.selection_set('0')
                self.show_record()
            else:
                self.summary.set('Входных записей нет.')
                put_text(self.details, 'В файле только комментарии и пустые строки. Выберите другой файл.')
            self.save_button.configure(state='normal')
            count = len(self.result['rows'])
            errors = sum(row['kind'] == 2 for row in self.result['rows'])
            self.status.set(f'Анализ завершён. Записей: {count}; ошибок ввода: {errors}.')

    def stop(self):
        if self.job:
            self.job.cancel()
            self.stop_button.configure(state='disabled')
            self.status.set('Остановка расчёта…')

    def show_record(self, _event=None):
        selection = self.records.selection()
        if not self.result or not selection:
            return
        row = self.result['rows'][int(selection[0])]
        self.methods.delete(*self.methods.get_children())
        lines = [f"Исходная строка: {row['line']}", f"n = {row['n']}", f"e = {row['e']}"]
        if row['kind'] == 2:
            self.summary.set('Ошибка ввода')
            lines.append(row['error'])
        elif row['kind'] == 1:
            self.summary.set(f"Информационный режим: n — {row['n_bits']} бит, e — {row['e_bits']} бит")
            lines += ['Факторизация, общие множители и соотношения RSA не проверялись.',
                      'Для вычислительного режима n и e должны помещаться в uint64_t.']
        else:
            self.summary.set(f"{row['u']} × {row['v']} = n" if row['has_factors'] else 'Разложение не найдено')
            lines += [f"Разрядность n: {row['bit_length']} бит.", f"Учебная модель: {MODELS[row['model_status']]}.",
                      f"Контрольные параметры: {CONTROLS[row['control_status']] }."]
            if row['has_factors']:
                lines.append('Два различных простых множителя: ' + ('да.' if row['prime_pair'] else 'нет.'))
            else:
                lines.append('Отсутствие разложения не доказывает стойкость ключа.')
            for method in row['methods']:
                index = method['method']
                elapsed = 'недоступно' if method['elapsed'] < 0 else f"{method['elapsed']:.6f}"
                self.methods.insert('', 'end', values=(METHODS[index], STATUSES[method['status']], method['iterations'], elapsed))
                lines.append(f"\n{METHODS[index]}: {STATUSES[method['status']]} — {REASONS[method['reason']] }.")
                if method['status'] == 0:
                    lines.append(f"Множители: {method['u']} × {method['v']}.")
            related = [p for p in self.result['pairs'] if row['line'] in (p['first_line'], p['second_line'])]
            for pair in related:
                kind = ('общий множитель', 'совпадающие модули — не факторизация', 'один модуль делит другой')[pair['kind']]
                lines.append(f"\nСтроки {pair['first_line']} и {pair['second_line']}: {kind}; НОД = {pair['gcd']}.")
        controls = [f'{key} = {value}' for key, value in row['reference'].items() if value is not None]
        if controls:
            lines.append('\nКонтрольный ввод: ' + ', '.join(controls))
        put_text(self.details, '\n'.join(lines))

    def save(self):
        if not self.result:
            return
        path = filedialog.asksaveasfilename(title='Сохранить завершённый отчёт под новым именем',
            defaultextension='.txt', initialfile=f"RSA_{datetime.now():%Y%m%d_%H%M%S}.txt",
            filetypes=[('Текстовый отчёт UTF-8', '*.txt')], confirmoverwrite=False)
        if path:
            try:
                with open(path, 'x', encoding='utf-8') as stream:
                    stream.write(f"Завершено: {self.result['completed']}\n")
                    stream.write(self.result['report'])
            except FileExistsError:
                messagebox.showerror('Файл уже существует', 'Выберите другое имя. Существующие файлы не перезаписываются.')
            except OSError as error:
                messagebox.showerror('Ошибка сохранения', str(error))
            else:
                self.status.set(f'Отчёт сохранён: {path}')

    @staticmethod
    def select_all(event):
        if isinstance(event.widget, (tk.Entry, ttk.Entry)):
            event.widget.selection_range(0, 'end')
            return 'break'
        if isinstance(event.widget, tk.Text):
            event.widget.tag_add('sel', '1.0', 'end-1c')
            return 'break'

    def scroll_page(self, event):
        if not isinstance(event.widget, (tk.Text, ttk.Treeview, ttk.Scrollbar)):
            page = self.nametowidget(self.tabs.select())
            page.canvas.yview_scroll(-int(event.delta / 120), 'units')

    def close(self):
        if self.poll_token is not None:
            self.after_cancel(self.poll_token)
            self.poll_token = None
        self.progress.stop()
        if self.job:
            self.job.close()
            self.job = None
        self.update_idletasks()
        self.destroy()
