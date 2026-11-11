#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ -x "$project/build-native/dfreernet" ]] || "$project/scripts/build.sh"
exec "$project/build-native/dfreernet" "$@"
