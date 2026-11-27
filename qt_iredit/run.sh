#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mode=${DFREERNET_BUILD_MODE:-RELEASE}
"$project/scripts/build.sh" "$mode"
exec "$project/build/$mode/qt_iredit" "$@"
