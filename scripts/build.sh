#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
export CCACHE_DISABLE=1
cmake -S "$project" -B "$project/build-native" -G Ninja -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$project/build-native" --parallel 2
file "$project/build-native/dfreernet"
