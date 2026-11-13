#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mode=${1:-${DFREERNET_BUILD_MODE:-RELEASE}}
case "$mode" in
  DEBUG|RELEASE) ;;
  *) echo 'Usage: scripts/build.sh [DEBUG|RELEASE]' >&2; exit 2 ;;
esac
build_dir="$project/build/$mode"
export CCACHE_DISABLE=1
cmake -S "$project" --preset "$mode"
cmake --build "$build_dir" --parallel 2
file "$build_dir/dfreernet"
