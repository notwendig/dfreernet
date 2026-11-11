#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
installation=$(cd -- "$project/.." && pwd)
export CCACHE_DISABLE=1
cmake -S "$project" -B "$project/build-native" -G Ninja -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_BUILD_TYPE=Debug
cmake --build "$project/build-native"
ctest --test-dir "$project/build-native" --output-on-failure
"$project/scripts/build-windows.sh"
python3 "$project/scripts/check_pe.py" "$project/build-windows/RNetReconstructed.exe"
(cd -- "$installation" && sha256sum --check --quiet "$project/originals.sha256")
if [[ "${1:-}" == "--static-only" ]]; then
  echo "Statische Tests bestanden. Wine, GUI und Controllerkommunikation wurden damit nicht geprüft."
  exit 0
fi
logdir=$(mktemp -d "$project/build-windows/gui-test-XXXXXX")
windows_output="Z:${logdir//\//\\}"
set +e
timeout 40s env WINEDEBUG=+timestamp,+loaddll "$project/scripts/run-wine.sh" --self-test --output "$windows_output" >"$logdir/wine.log" 2>&1
result=$?
set -e
(cd -- "$installation" && sha256sum --check --quiet "$project/originals.sha256")
if [[ "$result" != 0 ]]; then
  cat "$logdir/wine.log" >&2
  echo "Wine/GUI-Test fehlgeschlagen (Exit $result). Protokoll: $logdir" >&2
  exit "$result"
fi
if ! rg -q 'GUI_SELFTEST_PASSED' "$logdir/runtime.log" || ! rg -q 'SHUTDOWN clean' "$logdir/runtime.log"; then
  echo "Kein vollständiger GUI-/Beenden-Nachweis: $logdir" >&2
  exit 1
fi
printf 'GUI-Selbsttest bestanden. Controllerkommunikation bleibt ungetestet. Protokoll: %s\n' "$logdir"
