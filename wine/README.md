# Wine / ftd2xx.dll

[English](README.md) | [Deutsch](README.de.md)

A separate subproject for an existing 32-bit OEM Programmer installation:

```text
OEM Programmer / IRConfigurator (Wine)
  -> interface code (statically linked / DongleInterface.dll)
  -> ftd2xx.dll
       CRawMsg -> CUSBMsg: local dongle commands and responses
       CRawMsg -> CCANMsg: UDP to 127.0.0.1:39000
                            -> dfreernet-wine-can-proxy -> can0
       CAN RX <- proxy -> Raw24 with dongle RX checksum -> FT_Read
```

The `DONGLE` environment variable selects the emulated profile:

| Value | Access level |
| --- | --- |
| `Dealer` | 1 |
| `OEM` | 2 |
| `Factory` | 3 |

When `DONGLE` is unset, the default is `OEM`. Values are case-sensitive; empty
or unsupported values are rejected before opening the transport. The DLL reads
the variable once during initialization. Restart the Programmer to change the
profile.

The virtual EEPROM profile in `common/DongleIdentity.hpp` supplies the selected
level through USB `07/00`, address `0x86`; the corresponding identity CRC is
calculated and returned at address `0x8B`. The identity is held in the emulated
DLL's memory and initialized on every start.

The DLL emulates the FTDI/dongle interface. CAN/POP payloads are forwarded to
the bus; it does not start `rollstuhl.emu`. The inactive legacy replay/block
code inherited from the reference is not enabled. The DLL reports a connected
system only after receiving actual CAN traffic. USB status reports repository
node `0xFF` (unknown), allowing `DongleInterface.dll` to discover the repository
server using POP `40/80` on nodes 0..14. Reusing `0` from a single captured
session would fix the Programmer to node 0 and leave other controllers stuck
in initialization (repeated `20/80=0x1F`, POP abort `0x20`).

For the physical connection, follow the [connection guide](../README.md#connecting-a-wheelchair-to-a-computer-or-raspberry-pi), including the required galvanic isolation.

For native local IR menu editing, see [qt_iredit](../qt_iredit/README.md). It can open/save Pronto RnIR3 menus and accept your own Pronto Hex codes without starting Wine or a CAN proxy.

## Build and test offline

In addition to a native C++17 compiler, you need CMake, Ninja, MinGW-w64 i686
and Wine. The normal dfreernet build builds the native proxy and registers the
hardware-free conversion test. To build and check the Windows DLL:

```bash
wine/build.sh
python3 wine/tests/offline.py
scripts/test.sh
```

Outputs are confined to `build/wine/`. Set `DFREERNET_WINE_BUILD` to use a
different build directory. For an offline test with a different build directory,
pass that directory's `mingw32` subdirectory as an argument.

The Wine test loads the actual PE32 DLL, handles its UDP handshake itself, and
checks local USB responses, fragmented FT_Write calls, invalid checksums/DLC,
standard/extended/RTR frames, DLC 0, three-frame bursts and CAN RX. It also
checks all three `DONGLE` profiles, the OEM default with no variable, and
rejection of invalid values without starting the transport. It opens no CAN
socket. Wine uses the existing `WINEPREFIX`.
The main project's private-fixture tests are registered only when the fixtures
are available; the Wine subproject does not require them.

## Start the Programmer

Only with explicitly authorized use on a stationary wheelchair:

```bash
wine/run.sh --prepare-only  # prepare the private runtime copy only
DONGLE=OEM wine/run.sh      # start the Programmer and proxy for can0
DONGLE=Factory wine/run.sh  # alternatively: Factory identity
DONGLE=Dealer wine/run.sh   # alternatively: Dealer identity
```

Default original executable:
`~/.wine/drive_c/Program Files (x86)/PG Drives Technology/R-net Programmer OEM Generic/RNet Programmer6 OEM.exe`.
Override it with `--programmer /path/to/program.exe`, and select another
interface with `--interface canN`. The existing CAN interface must already be
configured and up (R-net: 125000 bit/s). The launcher does not configure it.

The launcher copies the Programmer runtime and resources to
`build/wine/runtime`, excluding old logs and ZIP archives. It replaces the DLL
only in that copy and forces `WINEDLLOVERRIDES=ftd2xx=n`. Installed originals
are preserved. Copied proprietary files remain in the ignored build directory
and must not be committed as project resources.

The launcher runs in the foreground. Closing the Programmer or pressing Ctrl+C
stops its associated proxy. Other Wine/emulator processes are not terminated.
Each run has separate logs under `build/wine/logs/<timestamp>/`:
`programmer.log`, `proxy.log` and, once the DLL is loaded, `ftd2xx.log`.

For a graphical OEM launcher, a desktop entry can invoke
`python3 /absolute/path/to/dfreernet/wine/start_oem.py` with `Terminal=false`.
This sets `DONGLE=OEM` and the normal `~/.wine` prefix, starts the verified OEM
Wine copy with its proxy, and prevents duplicate invocations of this launcher
in the same session. Startup failures appear in a dialog when Zenity is
available; the startup log is `build/wine/oem-launcher.log`. Starting the
installed EXE directly still bypasses this launch path.

`wine/start_ir.py` is the corresponding graphical launcher for the original
`IRConfigurator.exe`: OEM dongle identity, the same staged runtime, and its own
proxy lifecycle. Its startup log is `build/wine/ir-launcher.log`.
Use the OEM Programmer and IRConfigurator launchers one at a time; close the
running program before switching. An active proxy is detected before runtime
files are overwritten.

The Programmer can issue CAN write commands itself. The proxy does not enforce
read-only access. The existing Power-Off gate remains: after receiving standard
ID `002` as RTR, CAN TX stays blocked until the proxy restarts; `00C` does not
clear the gate.

## Local proxy protocol

Build the DLL and proxy together. The local `RNC2` UDP format is 20 bytes:
magic (0..3), type (4: Hello=1, TX=2, RX=3, Ack=4), DLC (5), little-endian CAN ID
(6..9), data (10..17), flags (18: bit 0 extended, bit 1 RTR), reserved (19, zero).
This preserves extended IDs below `0x800` and RTR. The port is `39000 + N` for
`canN`, N=0..999. The reference's earlier `RNC1` format is incompatible; do not
mix its DLL or proxy with this pair.

## Provenance

The files in `ftd2xx/`, `common/`, the toolchain and the initial proxy were taken
from the user-specified local project `/home/juergen/Projects/RNet-Dongle-Emu`,
commit `742913ccc668b1b32093a266e657f60b7c9f1cf9` (source without local changes).
Changes cover typed CUSBMsg/CCANMsg processing, DLC validation, EXT/RTR
transport, local peer validation, filter state, static MinGW runtime, tests and
the separate launch path. This records provenance and does not grant additional
rights to inherited or proprietary components.

## Download deadline under Wine

The launch path corrects the automatically calculated download deadline in both
the known `DongleInterface.dll` (IRConfigurator) and a separate OEM/Factory Wine
EXE. The OEM Programmer statically links the download code and does not load
`DongleInterface.dll`; correcting the DLL alone is insufficient for it.
The fixed allowance increases from 200 ms to 5000 ms in both download timing
models; the original 3 or 5 ms per payload byte remain unchanged. For 1070 bytes,
the shorter deadline becomes 8210 ms instead of 3410 ms. Uploads and the
Power-Off transmit gate remain unchanged. The installed original DLL is preserved.

Supported original SHA256:
`b45ad5e2579b4811cd6c48fd89d0e333c63fafd28d2c6ebf56b7144c35f7e829`.
Output SHA256:
`55c60b4a0c32ec24e8c0d246099dcdcab1e991f6695ba595178c0dcc080300f1`.
Other DLL versions are rejected before staging. Only the two timeout constants
in the automatic download path change; this invalidates the local copy's digital
signature. Proprietary binaries are not included.

The unmodified OEM/Factory staging copies are preserved. The launched executable
is `RNet Programmer6 OEM Wine.exe` or `RNet Programmer6 Factory Wine.exe`.
Only the two download constants at file offsets `0x24A1E` and `0x24A2A` are
additionally changed; adjacent upload deadlines remain untouched. Supported EXE
inputs are the known original OEM EXE documented below and the verified two-byte
Factory copy. Output checksums:

- OEM Wine: `9040c12d005b2917f1ea6eccf595cccba7fb6dcf1a97f5f31b5a2e27c01b51e3`
- Factory Wine: `9428cf1aa050480232e1e61aef8443c9b1a855563ffdbc105bb3567ebd100ac4`

Offline testing invoked the original DLL's actual download operation with 1070
synthetic bytes and delayed UDP segment acknowledgements. The original deadline
aborted early after about 3420 ms. The corrected copy transferred all 134
segments, including the final flag and independently verified CRC, in about
4400 ms. The corrected OEM Wine EXE was also tested in an isolated prefix:
1070/1070 bytes, 134 segments, final flag and independently verified CRC after
4458 ms. A DLL-only correction did not resolve the OEM abort because the EXE
statically links the code. Staging alone does not repair a file already damaged
in the controller.

```bash
python3 wine/tests/download_timeout_patch_test.py \
  --original '/path/to/original/DongleInterface.dll' \
  --programmer '/path/to/original/RNet Programmer6 OEM.exe'
DONGLE=OEM wine/run.sh --prepare-only
```

The launch path also copies available `libgcc_s_dw2-1.dll` and
`libwinpthread-1.dll` from `/usr/i686-w64-mingw32/sys-root/mingw/bin` beside the
runtime copy. The locally installed 32-bit Wine-Mono needs these dependencies
for `IRConfigurator.exe`; without libgcc the test launch exited with code 255.
Set `DFREERNET_MINGW32_RUNTIME_DIR` for a different library directory. Staging
does not modify the Wine prefix. With these libraries, the isolated
IRConfigurator test reached device selection; this did not verify full IR
communication.

## Separate Factory EXE copy

The known OEM EXE limits the effective level to
`min(application level 2, dongle level)`. The `--factory-exe` option additionally
creates `build/wine/runtime/RNet Programmer6 Factory.exe` with application level
`3`. The effective level still depends on the selected dongle: Dealer=1, OEM=2,
Factory=3.

```bash
DONGLE=Factory wine/run.sh --factory-exe --prepare-only
DONGLE=Factory wine/run.sh --factory-exe
```

`wine/patch_factory.py SOURCE.exe TARGET.exe` can also create the copy separately.
The patcher accepts only the verified source SHA256
`d5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec`.
It changes exactly two bytes: file offset `0x18B2F5`, the immediate operand of
the instruction at VA `0x0058BEEF` (`mov dword ptr [0x006CE33C], 2` becomes `3`),
and file offset `0x1D7AB5`, setting the MFC file dialog's `bVistaStyle` from TRUE
to FALSE so the copy uses the classic file dialog.
The output checksum is
`919deed14f206ddc7d23ad2429b0070f523fa895cfcdd8f0716ba1050a515751`.
An existing, exactly recognized older one-byte patch copy is backed up as
`.exe.v1.bak` and updated; unknown output files are rejected. The change
invalidates the original digital signature; this is a local modification.
It does not add Factory functions absent from the OEM version. Both EXE files
remain in the ignored build directory.

The patch guards run in normal CTest. To check against the local EXE:

```bash
python3 wine/tests/factory_patch_test.py --original 'build/wine/runtime/RNet Programmer6 OEM.exe'
wine build/wine/mingw32/wine_factory_limit_test.exe \
  'Z:\home\juergen\Projects\dfreernet\build\wine\runtime\RNet Programmer6 OEM.exe' \
  'Z:\home\juergen\Projects\dfreernet\build\wine\runtime\RNet Programmer6 Factory.exe'
```

The Windows checker executes only the extracted initialization and access-limit
instructions, redirecting the global address to test memory. It does not load
the Programmer or open a CAN connection. This does not verify a complete Factory
GUI session.

### Saving and the Wine registry

The isolated access-level check is not a full save test. The user attributed the
reported null-pointer access in the Wine file dialog while saving a `.R-net`
file to the Wine registry. The specific registry change is not documented here,
so no general registry repair is automated. Version 1.4.1 includes the optional
patcher, not an EXE.

After the registry repair, the current two-byte patch copy with `DONGLE=Factory`
was tested in a cloned Wine prefix: the GUI showed “Generic Factory”, promoted
a private file, completed “Save As” and reopened the saved file successfully.
The original file was unchanged and no crash log was produced. The test used
only local UDP replay without a SocketCAN proxy. The replay did not cover all
diagnostic queries; physical controller writes were not tested.

For targeted fault logging during a normal launch:

```bash
RNET_DEBUG_CRASH=1 DONGLE=Factory wine/run.sh --factory-exe
```

The optional DLL diagnostics log access violations with registers and stack
addresses to `build/wine/logs/<run>/ftd2xx.log.fault`. They change neither exception
handling nor CAN messages and do not require an attached WineDbg process.
A separate diagnostic copy also produced an SftTree license message; license
checks are not patched.
