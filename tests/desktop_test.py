#!/usr/bin/env python3
"""Exercise the real app on a private display with disposable local data."""
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def wait_until(predicate, message):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.1)
    raise AssertionError(message)


def main():
    if '--private-display' not in sys.argv:
        environment = os.environ.copy()
        for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
            environment.pop(name, None)
        subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1280x900x24',
                        sys.executable, __file__, '--private-display'],
                       env=environment, check=True, cwd=ROOT)
        return
    with tempfile.TemporaryDirectory(prefix='uku-test-') as directory:
        environment = os.environ.copy()
        environment['UKUVOTA_DATA_DIR'] = directory
        environment['SDL_RENDER_DRIVER'] = 'software'
        environment.pop('KRYON_CAPTURE_PATH', None)
        # Capture only the host's own first frame on this private display.
        artifacts = ROOT / 'build/smoke'
        artifacts.mkdir(parents=True, exist_ok=True)
        capture = artifacts / 'home.png'
        capture_environment = dict(environment, KRYON_CAPTURE_PATH=str(capture))
        subprocess.run([str(ROOT / 'build/uku-desktop')], env=capture_environment,
                       check=True, timeout=20, cwd=ROOT)
        assert capture.read_bytes().startswith(b'\x89PNG\r\n\x1a\n')
        database = Path(directory) / 'uku.sqlite3'
        with sqlite3.connect(database) as connection:
            connection.execute("insert into settings(key,value) values('theme_mode','1')")
        log = Path(directory) / 'app.log'
        with log.open('w') as output:
            app = subprocess.Popen([str(ROOT / 'build/uku-desktop')], env=environment,
                                   stdout=output, stderr=output, cwd=ROOT)
            try:
                window = ''
                def opened():
                    nonlocal window
                    result = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', '^Kryon Ziran$'],
                                            capture_output=True, text=True)
                    window = result.stdout.splitlines()[0] if result.stdout else ''
                    return bool(window)
                wait_until(opened, 'Our app did not open its window: ' + log.read_text())
                subprocess.run(['xdotool', 'windowsize', window, '1000', '760'], check=True)
                subprocess.run(['xdotool', 'windowfocus', window], check=True)
                def click(x, y):
                    subprocess.run(['xdotool', 'mousemove', '--window', window, str(x), str(y), 'mousedown', '1', 'sleep', '0.08', 'mouseup', '1'], check=True)
                    time.sleep(0.2)
                def type_text(value):
                    subprocess.run(['xdotool', 'type', '--clearmodifiers', '--delay', '20', value], check=True)
                    time.sleep(0.2)
                # Create and verify a process through the maintained wizard.
                click(650, 30)
                click(490, 248)
                type_text('Choose dinner')
                subprocess.run(['import', '-window', window, str(artifacts / 'setup.png')], check=True)
                click(650, 397)
                click(650, 392)  # Rules: weight, quorum and local storage notice
                click(650, 395)  # Timing: proposal and voting phases
                subprocess.run(['import', '-window', window, str(ROOT / 'build/smoke/review.png')], check=True)
                click(650, 375)  # Review: create the decision
                def created():
                    with sqlite3.connect(database) as connection:
                        return connection.execute("select count(*) from processes where topic='Choose dinner'").fetchone()[0] == 1
                wait_until(created, 'Wizard did not save the decision')
                with sqlite3.connect(database) as connection:
                    assert connection.execute("select count(*) from proposals where proposal_key in ('status-quo','repeat-process')").fetchone()[0] == 2
                assert app.poll() is None, log.read_text()
                # Verify the process view remains usable at a phone width.
                subprocess.run(['xdotool', 'windowsize', window, '360', '740'], check=True)
                time.sleep(0.3)
                subprocess.run(['import', '-window', window, str(artifacts / 'narrow.png')], check=True)
                assert app.poll() is None, log.read_text()
            finally:
                app.terminate()
                try:
                    app.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    app.kill()
                    app.wait()
        with sqlite3.connect(database) as connection:
            assert connection.execute("select value from settings where key='theme_mode'").fetchone() == ('1',)
    print('Desktop launch, private captures, navigation, resize and stored settings: pass')


if __name__ == '__main__':
    main()
