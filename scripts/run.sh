#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mode=${DFREERNET_BUILD_MODE:-RELEASE}
case "$mode" in
  DEBUG|RELEASE) ;;
  *) echo 'DFREERNET_BUILD_MODE must be DEBUG or RELEASE' >&2; exit 2 ;;
esac
program="$project/build/$mode/dfreernet"
if [[ ! -x "$program" ]] || [[ -n $(find "$project/src" "$project/translations" "$project/resources" "$project/CMakeLists.txt" "$project/scripts/build.sh" -type f -newer "$program" -print -quit) ]]; then
  "$project/scripts/build.sh" "$mode"
fi
exec "$program" "$@"
