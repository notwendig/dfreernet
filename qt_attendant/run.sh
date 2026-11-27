#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mode=${DFREERNET_BUILD_MODE:-RELEASE}
"$project/scripts/build.sh" "$mode"
# Diagnostic modes must never change network interfaces.
initialize_can=true
for argument in "$@"; do
    case "$argument" in
        --offline|--observe|--play-capture|--play-capture=*|-h|--help|--help-all|-v|--version|--screenshot|--screenshot=*|--quit-after|--quit-after=*)
            initialize_can=false ;;
    esac
done
if "$initialize_can"; then
    python3 "$project/qt_attendant/tools/setup_can.py" --bitrate "${RNET_BITRATE:-125000}"
fi
exec "$project/build/$mode/qt_attendant" "$@"
