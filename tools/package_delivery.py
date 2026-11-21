"""Архивы поставки и проверка распакованного приложения в другом каталоге."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
release = ROOT / 'release'
app = release / 'RSA-Analyzer'
archive = release / 'RSA-Analyzer-Windows-x64.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
    for path in sorted(app.rglob('*')):
        if path.is_file():
            output.write(path, 'RSA-Analyzer/' + path.relative_to(app).as_posix())

source_archive = release / 'RSA-Analyzer-source.zip'
folders = ('src', 'include', 'app', 'gui', 'tests', 'examples', 'assets', 'docs', 'tools')
with zipfile.ZipFile(source_archive, 'w', zipfile.ZIP_DEFLATED) as output:
    for folder in folders:
        for path in sorted((ROOT / folder).rglob('*')):
            if path.is_file() and '__pycache__' not in path.parts:
                output.write(path, 'rsa_analyzer/' + path.relative_to(ROOT).as_posix())
    for name in ('Makefile', 'README.md', 'build_gui.ps1', '.gitignore'):
        output.write(ROOT / name, 'rsa_analyzer/' + name)

with tempfile.TemporaryDirectory(prefix='delivery-check-', dir=ROOT / 'build') as temporary:
    target = Path(temporary).resolve()
    assert target.is_relative_to((ROOT / 'build').resolve())
    with zipfile.ZipFile(archive) as source:
        assert source.testzip() is None
        source.extractall(target)
    request = dict(mode='key', source='261883 3', settings=dict(
        trial_bound='100000', pollard_bound='10000', pollard_base='2', iterations='100000',
        seconds='1', selected=[True, True, True], compare=True))
    request_path, result_path = target / 'request.json', target / 'result.json'
    request_path.write_text(json.dumps(request), encoding='utf-8')
    environment = os.environ.copy()
    for name in ('PYTHONPATH', 'PYTHONHOME', 'TCL_LIBRARY', 'TK_LIBRARY'):
        environment.pop(name, None)
    environment['PATH'] = str(Path(environment.get('SystemRoot', 'C:/Windows')) / 'System32')
    subprocess.run([str(target / 'RSA-Analyzer/RSA-Analyzer.exe'), '--worker',
                    str(request_path), str(result_path)], cwd=target, env=environment,
                   timeout=30, check=True)
    result = json.loads(result_path.read_text(encoding='utf-8'))
    assert result['ok'], result
    assert result['result']['rows'][0]['u'] == 257
    assert result['result']['rows'][0]['v'] == 1019

print('Extracted delivery, isolated PATH, Pollard example: passed')
for path in (archive, source_archive):
    print(path.name, path.stat().st_size, 'bytes')
