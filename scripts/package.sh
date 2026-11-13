#!/usr/bin/env bash
# Local relocatable application directory; no proprietary executables or DLLs.
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mode=${DFREERNET_BUILD_MODE:-RELEASE}
"$project/scripts/build.sh" "$mode"
cmake --install "$project/build/$mode" --prefix "$project/dist/dfreernet"
install -d "$project/dist/dfreernet/share/doc/dfreernet"
cp -- "$project"/README{,.de}.md "$project"/CHANGELOG{,.de}.md "$project"/LICENSE "$project"/SCOPE{,.de}.md "$project/dist/dfreernet/share/doc/dfreernet/"
install -d "$project/dist/dfreernet/share/doc/dfreernet/docs"
cp -- "$project"/docs/*.md "$project/dist/dfreernet/share/doc/dfreernet/docs/"
install -d "$project/dist/dfreernet/share/doc/dfreernet/resources"
cp -- "$project"/resources/PROVENANCE*.md "$project/dist/dfreernet/share/doc/dfreernet/resources/"
printf '%s\n' "Standalone application directory: $project/dist/dfreernet" "Run: dist/dfreernet/bin/dfreernet (requires installed Qt 6 and zlib)."
