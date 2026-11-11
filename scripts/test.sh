#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$project/analysis"
"$project/scripts/build.sh"
ctest --test-dir "$project/build-native" --output-on-failure
if ldd "$project/build-native/dfreernet" | rg -i 'not found|wine|ftd2xx|CUSBMsg'; then
  echo 'Unzulässige oder fehlende Laufzeitabhängigkeit' >&2
  exit 1
fi
echo 'Native Qt-6-Linux-App und Dateifunktionen geprüft; kein physischer CAN-Zugriff im Test.'
