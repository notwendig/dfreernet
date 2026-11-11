#!/usr/bin/env bash
# Original software testing: private prefix, no physical devices, isolated network.
set -euo pipefail
prefix=/tmp/rnet-wine-isolated
[[ -f "$prefix/system.reg" ]] || { echo 'Isolated prefix not prepared' >&2; exit 1; }
exec bwrap --unshare-net --die-with-parent --ro-bind / / --dev /dev --proc /proc \
  --bind /tmp /tmp --bind "$prefix" "$prefix" \
  --setenv WINEPREFIX "$prefix" --setenv WINEDEBUG "${WINEDEBUG:--all}" \
  --setenv DBUS_SESSION_BUS_ADDRESS disabled: \
  "$@"
