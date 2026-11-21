"""Проверки DLL, реального рабочего процесса и границы Python/C."""
import copy
import re
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'gui'))
from bridge import analyze, library, DEFAULTS, validate_settings
from jobs import AnalysisJob


class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(dir=ROOT / 'build')
        self.path = Path(self.folder.name)
        self.counter = 0

    def tearDown(self):
        self.folder.cleanup()

    def analyze(self, source, mode='key', **settings):
        self.counter += 1
        return analyze(dict(mode=mode, source=source, settings=dict(copy.deepcopy(DEFAULTS), **settings)),
                       self.path / f'report{self.counter}.txt')

    def test_layout_and_methods(self):
        library()  # Каждый размер и каждое смещение сверяются с компилятором C.
        for source in ('10403 7', '261883 3', '18446743979220271189 65537'):
            result = self.analyze(source, compare=True)
            row = result['rows'][0]
            self.assertTrue(row['has_factors'])
            self.assertEqual(row['u'] * row['v'], int(row['n']))
            self.assertEqual(len(row['methods']), 3)

    def test_console_equivalence(self):
        source = '10403 7'
        result = self.analyze(source, compare=True)
        cli = subprocess.run([str(ROOT / 'build/rsa_analyzer.exe'), '--key', source, '--compare'],
                             encoding='utf-8', capture_output=True, check=True).stdout
        normalize = lambda text: re.sub(r'время=\d+\.\d+ с', 'время=<измерение> с', text)
        self.assertEqual(normalize(cli), normalize(result['report']))

    def test_large_and_controls(self):
        for value in (2**64, 10**1023):
            row = self.analyze(f'{value} 3')['rows'][0]
            self.assertEqual(row['kind'], 1)
            self.assertEqual(row['n_bits'], value.bit_length())
        result = self.analyze('3233 17 - 53 2753')
        self.assertEqual(result['rows'][0]['control_status'], 2)
        self.assertIn('d=2753', result['report'])
        self.assertEqual(self.analyze('3233 17 - - 0')['rows'][0]['control_status'], 3)

    def test_files_and_errors(self):
        source = self.path / 'ключи.txt'
        source.write_text('\ufeff# тест\n3233 17\nошибка\n3599 17\n3233 17\n', encoding='utf-8')
        result = self.analyze(str(source), mode='file')
        self.assertEqual([r['kind'] for r in result['rows']], [0, 2, 0, 0])
        self.assertTrue(any(p['first_line'] == 2 and p['second_line'] == 4 and p['gcd'] == 61 for p in result['pairs']))
        self.assertTrue(any(p['kind'] == 1 for p in result['pairs']))
        source.write_bytes(b'9' * 6000 + b'\n10403 7\n')
        self.assertEqual([r['kind'] for r in self.analyze(str(source), mode='file')['rows']], [2, 0])
        source.write_text('10403 7\n' * 257, encoding='utf-8')
        with self.assertRaises(ValueError):
            self.analyze(str(source), mode='file')
        source.write_text('# пустой\n', encoding='utf-8')
        self.assertEqual(self.analyze(str(source), mode='file')['rows'], [])

    def test_limits_and_invalid_settings(self):
        self.assertIn('iterations', validate_settings(dict(DEFAULTS, iterations=str(2**64))))
        self.assertIn('seconds', validate_settings(dict(DEFAULTS, seconds='nan')))
        with self.assertRaises(ValueError):
            self.analyze('10403 7', pollard_base='-1')
        row = self.analyze('1000036000099 65537', iterations='1', compare=True)['rows'][0]
        self.assertEqual(row['methods'][0]['reason'], 3)

    def test_worker_lifecycle(self):
        request = dict(mode='key', source='10403 7', settings=dict(DEFAULTS, compare=True))
        job = AnalysisJob(request)
        folder = job.root
        try:
            payload = None
            deadline = time.monotonic() + 15
            while payload is None and time.monotonic() < deadline:
                payload = job.poll()
                time.sleep(0.02)
            self.assertTrue(payload['ok'], payload)
            self.assertEqual(payload['result']['rows'][0]['u'], 101)
            self.assertFalse(folder.exists())
        finally:
            job.close()
        request['source'] = '18446744073709551557 65537'
        request['settings'] = dict(DEFAULTS, iterations=str(2**64 - 1), seconds='60',
                                   trial_bound=str(2**64 - 1), selected=[True, False, False])
        job = AnalysisJob(request)
        try:
            time.sleep(0.2)
            job.cancel()
            deadline = time.monotonic() + 5
            payload = None
            while payload is None and time.monotonic() < deadline:
                payload = job.poll()
                time.sleep(0.02)
            self.assertTrue(payload['cancelled'])
            self.assertFalse(job.root.exists())
        finally:
            job.close()


if __name__ == '__main__':
    unittest.main(verbosity=2)
