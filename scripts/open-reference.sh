#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
exec "$project/scripts/run.sh" --document "$project/tests/fixtures/original-reference-default.R-net" "$@"
