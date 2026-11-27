#!/usr/bin/env bash
# Passive acquisition only; no CAN configuration or transmission.
set -euo pipefail
usage() {
    echo 'Usage: qt_attendant/tools/capture.sh PHASE [INTERFACE [SECONDS]]'
    echo 'PHASE: startup, passenger, takeover, return'
    echo 'Defaults: can0, 60 seconds. Saves private logs under build/attendant-captures/.'
}
if [[ ${1:-} == --help ]]; then usage; exit 0; fi
phase=${1:-}
interface=${2:-can0}
seconds=${3:-60}
case "$phase" in startup|passenger|takeover|return) ;; *) usage >&2; exit 2 ;; esac
if (( $# > 3 )) || [[ ! $interface =~ ^[a-zA-Z0-9_][a-zA-Z0-9_.-]{0,14}$ ]] || [[ ! $seconds =~ ^[1-9][0-9]{0,2}$ ]] || (( seconds > 300 )); then
    usage >&2; exit 2
fi
command -v candump >/dev/null
command -v timeout >/dev/null
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
umask 077
mkdir -p "$project/build/attendant-captures"
output=$(mktemp "$project/build/attendant-captures/${phase}-XXXXXXXX.log")
echo "Receive-only capture: $interface, ${seconds}s, phase=$phase"
echo "Output: $output"
status=0
timeout --signal=INT --kill-after=2s "${seconds}s" candump -L -r 1048576 "$interface" > "$output" || status=$?
if (( status != 0 && status != 124 && status != 130 )); then
    echo "Capture failed (status $status); preserve partial log: $output" >&2
    exit "$status"
fi
if [[ ! -s $output ]]; then
    echo 'No frames received. Check the adapter, interface and bus connection.' >&2
    exit 1
fi
echo "Capture saved: $output"
