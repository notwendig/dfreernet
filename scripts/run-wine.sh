#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
installation=$(cd -- "$project/.." && pwd)
exe="$project/build-windows/RNetReconstructed.exe"
[[ -f "$exe" ]] || "$project/scripts/build-windows.sh"
(cd -- "$installation" && sha256sum --check --quiet "$project/originals.sha256")
# The application no longer imports FTD2XX. Keep the original installation on
# the search path for the separately requested, optional FTDI analysis action.
# The direct CAN backend never loads or calls that DLL.
windows_installation=$(winepath --windows "$installation")
export WINEPATH="$windows_installation${WINEPATH:+;$WINEPATH}"
exec wine "$exe" --installation "$windows_installation" "$@"
