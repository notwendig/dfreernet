#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mode=${DFREERNET_BUILD_MODE:-RELEASE}
case "$mode" in
  DEBUG|RELEASE) ;;
  *) echo 'DFREERNET_BUILD_MODE must be DEBUG or RELEASE' >&2; exit 2 ;;
esac
build_dir="$project/build/$mode"
mkdir -p "$project/analysis"
"$project/scripts/build.sh" "$mode"
ctest --test-dir "$build_dir" --output-on-failure
if ldd "$build_dir/dfreernet" | rg -i 'not found|wine|ftd2xx|CUSBMsg'; then
  echo 'Unzulässige oder fehlende Laufzeitabhängigkeit' >&2
  exit 1
fi
echo 'Native Qt-6-Linux-App und Dateifunktionen geprüft; kein physischer CAN-Zugriff im Test.'
