#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
for tool in cmake ninja i686-w64-mingw32-g++ i686-w64-mingw32-windres; do
  command -v "$tool" >/dev/null || { echo "Fehlendes Werkzeug: $tool" >&2; exit 1; }
done
export CCACHE_DISABLE=1
cmake -S "$project" -B "$project/build-windows" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$project/cmake/mingw-i686.cmake" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF
cmake --build "$project/build-windows"
file "$project/build-windows/RNetReconstructed.exe"
