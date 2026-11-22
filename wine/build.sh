#!/usr/bin/env bash
set -euo pipefail
wine_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
wine_build="${DFREERNET_WINE_BUILD:-$wine_root/../build/wine}"
cmake -S "$wine_root" -B "$wine_build/native" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$wine_build/native" --parallel 2
ctest --test-dir "$wine_build/native" --output-on-failure
cmake -S "$wine_root" -B "$wine_build/mingw32" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$wine_root/cmake/mingw32.cmake" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$wine_build/mingw32" --parallel 2
