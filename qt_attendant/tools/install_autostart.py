#!/usr/bin/env python3
"""Install the local BGM binary in the current user's XDG login autostart."""
import argparse
import os
from pathlib import Path
import tempfile

MARKER = 'X-DFreeRNet-Autostart=true'


def desktop_entry(binary):
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise ValueError(f'Build the application first: {binary}')
    # No shell is involved. Restrict field-code/escape characters so the quoted
    # Desktop Entry Exec path remains a literal executable, also with spaces.
    if any(c in str(binary) for c in '\n\r%"\\$`'):
        raise ValueError('Executable path contains unsupported desktop-entry characters')
    return f'''[Desktop Entry]
Type=Application
Name=R-Net Control
Name[de]=R-Net Steuerung
Comment=Start the BGM emulator in the system tray at login
Comment[de]=BGM-Emulator bei der Anmeldung im Infobereich starten
Exec="{binary}" --start-in-tray --minimize-to-tray
Icon={binary.parents[2]/'qt_attendant/icons/dfreernet-attendant.svg'}
Terminal=false
StartupNotify=false
X-GNOME-Autostart-enabled=true
{MARKER}
'''


def install(destination, content):
    if destination.is_symlink():
        raise ValueError(f'Refusing to replace a symlink: {destination}')
    if destination.exists() and MARKER not in destination.read_text().splitlines():
        raise ValueError(f'Refusing to replace an unrelated entry: {destination}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8',
                                         prefix='.dfreernet-', dir=destination.parent,
                                         delete=False) as output:
            temporary = Path(output.name)
            output.write(content)
            os.fchmod(output.fileno(), 0o644)
        temporary.replace(destination)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('RELEASE', 'DEBUG'), default='RELEASE')
    parser.add_argument('--output', type=Path, help='Override the user autostart file (for preview checks).')
    parser.add_argument('--preview', action='store_true', help='Print the entry without writing it.')
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[2]
    config = Path(os.environ.get('XDG_CONFIG_HOME') or Path.home()/'.config')
    destination = args.output or config/'autostart/dfreernet-attendant.desktop'
    try:
        if not destination.is_absolute():
            raise ValueError('The autostart destination must be absolute')
        content = desktop_entry(project/'build'/args.mode/'qt_attendant')
        if args.preview:
            print(content, end='')
        else:
            install(destination, content)
            print('Installed:', destination)
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
