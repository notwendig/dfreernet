#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
"$project/scripts/prepare-isolated-prefix.sh"
mkdir -p "$project/analysis"
trace="$project/analysis/original-$(date +%Y%m%d-%H%M%S).log"
printf 'Wine trace: %s\n' "$trace"
exec env WINEDEBUG=+timestamp,+loaddll,+thread,+file,+reg LIBGL_ALWAYS_SOFTWARE=1 \
  "$project/scripts/isolated-wine.sh" wine 'C:\Program Files (x86)\PG Drives Technology\R-net Programmer OEM Generic\RNet Programmer6 OEM.exe' >"$trace" 2>&1
