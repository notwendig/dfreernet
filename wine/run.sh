#!/usr/bin/env bash
set -euo pipefail
wine_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$wine_root/run.py" "$@"
