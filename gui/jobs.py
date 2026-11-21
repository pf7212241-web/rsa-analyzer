"""Один изолированный расчёт; окно опрашивает процесс без блокировки."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from datetime import datetime


def worker_main(request_path, output_path):
    from bridge import analyze
    target = Path(output_path)
    try:
        request = json.loads(Path(request_path).read_text(encoding='utf-8'))
        result = analyze(request, target.with_suffix('.txt'))
        result['completed'] = datetime.now().astimezone().isoformat(timespec='seconds')
        payload = dict(ok=True, result=result)
    except Exception as error:
        payload = dict(ok=False, error=str(error))
    temporary = target.with_suffix('.tmp')
    temporary.write_text(json.dumps(payload, ensure_ascii=False), encoding='utf-8')
    temporary.replace(target)


class AnalysisJob:
    def __init__(self, request):
        self.folder = tempfile.TemporaryDirectory(prefix='rsa-analysis-')
        self.root = Path(self.folder.name)
        self.cancelled = False
        self.closed = False
        request_file = self.root / 'request.json'
        request_file.write_text(json.dumps(request, ensure_ascii=False), encoding='utf-8')
        command = [sys.executable]
        if not getattr(sys, 'frozen', False):
            command.append(str(Path(__file__).with_name('main.py')))
        command += ['--worker', str(request_file), str(self.root / 'result.json')]
        try:
            self.process = subprocess.Popen(command, stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        except Exception:
            self.folder.cleanup()
            raise

    def poll(self):
        if self.process.poll() is None:
            return None
        try:
            if self.cancelled:
                return dict(ok=False, cancelled=True)
            target = self.root / 'result.json'
            if not target.exists():
                return dict(ok=False, error=f'Процесс анализа завершился без результата (код {self.process.returncode}).')
            return json.loads(target.read_text(encoding='utf-8'))
        finally:
            self.close()

    def cancel(self):
        self.cancelled = True
        if self.process.poll() is None:
            self.process.terminate()

    def close(self):
        if not self.closed:
            if self.process.poll() is None:
                self.process.kill()
            self.process.wait(timeout=5)
            self.folder.cleanup()
            self.closed = True
