#!/usr/bin/env bash
# Private test environment only; never deploys/replaces an installed DLL.
set -euo pipefail
umask 077
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
installation=$(cd -- "$project/.." && pwd)
source_prefix=${installation%%/drive_c/*}
prefix=/tmp/rnet-wine-isolated
for tool in bwrap wine cp cmp; do command -v "$tool" >/dev/null; done
[[ -f "$source_prefix/system.reg" ]] || { echo 'Cannot identify source Wine prefix' >&2; exit 1; }
(cd "$installation" && sha256sum --check --quiet "$project/originals.sha256")
mkdir -p "$prefix/drive_c" "$prefix/dosdevices"
chmod 700 "$prefix"
for name in system.reg user.reg userdef.reg; do
  [[ -e "$prefix/$name" ]] || cp --reflink=auto "$source_prefix/$name" "$prefix/$name"
done
[[ -d "$prefix/drive_c/windows" ]] || cp -a --reflink=auto "$source_prefix/drive_c/windows" "$prefix/drive_c/"
ln -sfn "$prefix/drive_c" "$prefix/dosdevices/c:"
ln -sfn / "$prefix/dosdevices/z:"
target="$prefix/drive_c/Program Files (x86)/PG Drives Technology/R-net Programmer OEM Generic"
[[ ! -L "$target" ]] || { echo 'Refuse linked application runtime: physical copies required' >&2; exit 1; }
mkdir -p "$target"
shopt -s nullglob
files=("$installation"/*.exe "$installation"/*.dll "$installation"/*.rnd "$installation"/Configure.bin "$installation"/*.png "$installation"/*.HLP "$installation"/*.cnt)
for source in "${files[@]}"; do
  name=${source##*/}
  if [[ ! -e "$target/$name" ]]; then
    cp --reflink=auto --preserve=mode,timestamps "$source" "$target/$name"
  elif [[ "$name" == *.dll || "$name" == *.exe || "$name" == *.rnd ]]; then
    cmp --silent "$source" "$target/$name" || { echo "Private runtime differs: $name; refusing replacement" >&2; exit 1; }
  fi
done
"$project/scripts/isolated-wine.sh" sh -c 'test ! -e /dev/bus/usb && test "$(ip -o link show | wc -l)" = 1'
"$project/scripts/isolated-wine.sh" wine reg add 'HKCU\Software\PG Drives Technology\Rnet Programmer OEM Generic\Settings' /v FTDIActive /t REG_DWORD /d 0 /f
"$project/scripts/isolated-wine.sh" wine reg add 'HKCU\Software\PG Drives Technology\Rnet Programmer OEM Generic\Settings' /v 'Program Support Directory' /t REG_SZ /d 'C:\Program Files (x86)\PG Drives Technology\R-net Programmer OEM Generic' /f
printf 'Private prefix prepared: %s\n' "$prefix"
