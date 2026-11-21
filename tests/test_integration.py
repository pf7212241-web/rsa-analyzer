"""Независимые целочисленные эталоны и проверка пользовательских сценариев."""
from pathlib import Path
import math
import random
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/rsa_analyzer.exe'

def run(*args, text=None, code=0):
    result = subprocess.run([str(EXE), *map(str, args)], input=text,
                            encoding='utf-8', capture_output=True, timeout=30)
    assert result.returncode == code, (args, result.returncode, result.stderr)
    return result.stdout

def main():
    rng = random.Random(20261109)
    cases = [(0, 0, 1), (2**64-1, 2**64-1, 2**64-1),
             (2**64-1, 2**64-2, 2**64-59)]
    cases += [(rng.getrandbits(64), rng.getrandbits(64),
               rng.randrange(1, 2**64)) for _ in range(1200)]
    result = subprocess.run([str(ROOT/'build/math_probe.exe')],
        input=''.join(f'{a} {b} {m}\n' for a,b,m in cases),
        text=True, capture_output=True, check=True, timeout=60)
    lines = result.stdout.splitlines()
    assert len(lines) == len(cases)
    for (a,b,m), line in zip(cases, lines):
        assert list(map(int, line.split())) == [math.gcd(a,b), a*b % m,
            pow(a,b,m), math.isqrt(a), a.bit_length()], (a,b,m,line)
    print(f'Independent arithmetic checks: {len(cases)*5}')
    out = run('--key', '10403 7', '--compare')
    assert '101 * 103' in out and 'независимое сравнение' in out
    out = run('--key', '18446744073709551616 65537')
    assert 'n — 65 бит' in out
    out = run('--key', f'{10**1023} 3')
    assert f'n — {(10**1023).bit_length()} бит' in out
    out = run('--key', '00018446744073709551616 0003')
    assert 'n — 65 бит' in out and 'e — 2 бит' in out
    run('--key', '10403 7', '--iterations', '0', code=2)
    run('--key', '10403 7', '--seconds', 'nan', code=2)
    run('--key', '10403 7', '--methods', '8', code=2)
    for key in ['0 3', '15 1', '-15 3', '15 3 5', '15 3 3 5 18446744073709551616',
                '1'*1025 + ' 3', '15 3 3 5 3 extra']:
        assert 'Ошибка ввода' in run('--key', key, code=2)
    out = run('--key', '3233 17 - 53 2753')
    assert 'Контрольные параметры: подтверждены' in out
    out = run('--key', '3233 17 - - 2753', '--methods', '0')
    assert 'отложена до факторизации' in out
    with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
        folder = Path(directory)
        source = folder/'ключи.txt'
        report = folder/'отчёт.txt'
        source.write_text('\ufeff# Пример\n10403 7\n\nне число\n' +
                          '18446744073709551616 3\n3233 17\n3599 17\n', encoding='utf-8')
        out = run('--file', source, '--compare', '--output', report, code=2)
        assert 'строка 4' in out and 'Ошибка ввода' in out
        assert 'Строки 6 и 7' in out and 'НОД=61' in out
        assert report.read_text(encoding='utf-8') == out
        before = source.read_bytes()
        run('--file', source, '--output', source, code=3)
        assert source.read_bytes() == before
        source.write_text('9'*6000 + '\n10403 7\n', encoding='utf-8')
        out = run('--file', source, code=2)
        assert 'слишком длинная' in out.lower() and '101 * 103' in out
        source.write_bytes(b'10403\x00 7\n10403 7\n')
        out = run('--file', source, code=2)
        assert 'нулевой байт' in out and '101 * 103' in out
        source.write_text('10403 7\n'*257, encoding='utf-8')
        run('--file', source, code=2)
        source.write_text('# Пусто\n\n', encoding='utf-8')
        assert 'Входных записей нет' in run('--file', source)
    out = run(text='1\n10403 7\n\n0\n')
    assert '101 * 103' in out
    out = run(text='3\n2\n\n\n\n100\n1\n1\n1\n10403 7\n\n0\n')
    assert 'Настройки сохранены' in out and 'независимое сравнение' in out
    print('CLI, Unicode paths, file errors, reports, menu: passed')

if __name__ == '__main__':
    main()
