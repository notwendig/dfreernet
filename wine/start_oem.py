#!/usr/bin/env python3
"""Desktop entry points for the OEM Programmer/IR Configurator and CAN proxy."""
import fcntl
import os
from pathlib import Path
import shutil
import subprocess
import sys


def show_error(title, message):
    dialog = shutil.which('zenity')
    if dialog:
        subprocess.run([dialog, '--error', '--title=' + title,
                        '--text=' + message], check=False)
    else:
        print(message, file=sys.stderr)


def main(kind='oem'):
    if kind not in ('oem', 'ir'):
        raise ValueError('Unknown desktop launcher')
    title = 'IR Configurator' if kind == 'ir' else 'R-net Programmer OEM'
    root = Path(__file__).resolve().parent
    build = Path(os.environ.get('DFREERNET_WINE_BUILD', root.parent / 'build/wine')).resolve()
    build.mkdir(parents=True, exist_ok=True)
    log_path = build / ('ir-launcher.log' if kind == 'ir' else 'oem-launcher.log')
    with (build / 'oem-launcher.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            show_error(title, 'Ein OEM-Programmer oder IRConfigurator ist bereits aktiv.\n'
                       'Bitte dieses Fenster zuerst schließen und danach erneut starten.')
            return 1
        env = dict(os.environ, DONGLE='OEM', WINEPREFIX=str(Path.home() / '.wine'))
        command = [sys.executable, str(root / 'run.py')]
        if kind == 'ir':
            source = (Path.home() / '.wine/drive_c/Program Files (x86)/PG Drives Technology'
                      / 'R-net Programmer OEM Generic/IRConfigurator.exe')
            command += ['--programmer', str(source)]
        command += sys.argv[1:]
        with log_path.open('w') as log:
            try:
                result = subprocess.run(command,
                                        cwd=root.parent, env=env, stdout=log,
                                        stderr=subprocess.STDOUT)
                code = result.returncode
            except OSError as error:
                log.write(str(error) + '\n')
                code = 1
        if code:
            show_error(title, f'Start fehlgeschlagen (Code {code}).\nDetails: {log_path}')
        return code


if __name__ == '__main__':
    sys.exit(main())
