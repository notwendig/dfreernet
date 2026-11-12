#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
program="$project/build-native/dfreernet"
if [[ ! -x "$program" ]] || [[ -n $(find "$project/src" "$project/translations" "$project/resources" "$project/CMakeLists.txt" "$project/scripts/build.sh" -type f -newer "$program" -print -quit) ]]; then
  "$project/scripts/build.sh"
fi
exec "$program" "$@"
