"""Сценарии Tk: настройки, пример, результат, отмена, сохранение, закрытие."""
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'gui'))
from window import Application


class WindowTests(unittest.TestCase):
    def setUp(self):
        self.app = Application()
        self.app.withdraw()

    def tearDown(self):
        self.app.close()

    def finish(self):
        deadline = time.monotonic() + 15
        while self.app.job and time.monotonic() < deadline:
            self.app.update()
            time.sleep(0.02)
        self.assertIsNone(self.app.job)

    def test_example_result_and_stale_report(self):
        app = self.app
        app.example('fermat')
        app.start()
        self.finish()
        self.assertEqual(app.result['rows'][0]['u'], 101)
        self.assertEqual(len(app.methods.get_children()), 3)
        app.values['n'].set('3233')
        self.assertIn('предыдущий', app.stamp.get())
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as directory:
            target = Path(directory) / 'отчёт.txt'
            with patch('window.filedialog.asksaveasfilename', return_value=str(target)):
                app.save()
                self.assertIn('n=10403', target.read_text(encoding='utf-8'))
                before = target.read_bytes()
                with patch('window.messagebox.showerror') as error:
                    app.save()
                    error.assert_called_once()
                self.assertEqual(target.read_bytes(), before)

    def test_input_and_settings_validation(self):
        app = self.app
        app.start()
        self.assertIsNone(app.job)
        self.assertIn('n:', app.form_error.get())
        app.example('fermat')
        app.settings['seconds'].set('nan')
        app.start()
        self.assertTrue(app.setting_errors['seconds'].get())
        self.assertIsNone(app.job)
        app.defaults()
        app.values['n'].set(str(2**64))
        app.start()
        self.finish()
        self.assertEqual(app.result['rows'][0]['n_bits'], 65)
        self.assertEqual(app.methods.get_children(), ())

    def test_example_cancel_and_close_worker(self):
        app = self.app
        app.values['n'].set('99')
        with patch('window.messagebox.askyesno', return_value=False):
            app.example('fermat')
        self.assertEqual(app.values['n'].get(), '99')
        with patch('window.messagebox.askyesno', return_value=True):
            app.example('pollard')
        self.assertEqual(app.values['n'].get(), '261883')
        app.values['n'].set('18446744073709551557')
        app.values['e'].set('65537')
        app.settings['iterations'].set(str(2**64 - 1))
        app.settings['trial_bound'].set(str(2**64 - 1))
        app.settings['seconds'].set('60')
        app.start()
        process = app.job.process
        app.stop()
        self.finish()
        self.assertIsNotNone(process.poll())
        self.assertIsNone(app.result)
        app.start()
        process = app.job.process
        app.close()
        self.assertIsNotNone(process.poll())
        self.app = Application()
        self.app.withdraw()

    def test_file_selection_and_empty_file(self):
        app = self.app
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as directory:
            source = Path(directory) / 'набор.txt'
            source.write_text('3233 17\nошибка\n3599 17\n', encoding='utf-8')
            app.mode.set('file')
            app._source_mode()
            with patch('window.filedialog.askopenfilename', return_value=str(source)):
                app.choose_file()
            app.start()
            self.finish()
            self.assertEqual(len(app.records.get_children()), 3)
            app.records.selection_set('1')
            app.show_record()
            self.assertEqual(app.summary.get(), 'Ошибка ввода')
            app.records.selection_set('0')
            app.show_record()
            self.assertIn('НОД = 61', app.details.get('1.0', 'end'))
            source.write_text('# пусто\n', encoding='utf-8')
            app.start()
            self.finish()
            self.assertEqual(app.records.get_children(), ())
            self.assertEqual(app.methods.get_children(), ())
            self.assertIn('нет', app.summary.get())


if __name__ == '__main__':
    unittest.main(verbosity=2)
