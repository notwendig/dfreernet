# Development

[English](DEVELOPMENT.md) | [Deutsch](DEVELOPMENT.de.md)

## Build and run

The supported development frontend is native Linux, C++17, CMake 3.21+, Ninja, Python 3, zlib development files and Qt 6.10+ (Widgets, Sql, DBus, LinguistTools; Quick, QuickControls2 for `qt_attendant`; Test and QuickTest for tests). No additional network services are required at runtime. Qt 6.11.2 is installed on the current development host.

```bash
scripts/build.sh
scripts/test.sh
scripts/run.sh --language en_US
```

CMake target and binaries: `dfreernet`, `build/RELEASE/dfreernet` and `build/DEBUG/dfreernet`. `scripts/build.sh` defaults to RELEASE (`RelWithDebInfo`); `scripts/build.sh DEBUG` uses CMake `Debug`. Set `DFREERNET_BUILD_MODE=DEBUG` for launch, test, package, and Python integration scripts. Scripts use two build jobs. Compiler warnings are enabled for native source targets. For manual CMake use, provide the same Qt modules and use an out-of-source build.

In VSCodium, reload the window and select the **DEBUG** or **RELEASE** configure preset in the CMake Tools project status panel. Its Build, Test, Debug and Launch controls then use the corresponding `build/` directory.

Native runtime resources are embedded from checked project files in `resources/`. CMake runs `scripts/embed_resources.py`, which checks their pinned SHA256 values and creates a C++ translation unit. The main program requires explicit acceptance of its startup warning before constructing the document/controller UI. `--quit-after` can reject this warning for UI checks; it cannot accept it.

## Source map

| Location | Responsibility |
| --- | --- |
| `src/qt/` | Widgets, translations, settings, typed editors, icons, startup notice, passive heartbeat |
| `src/document.*`, `src/database.*`, `src/values.*` | File preservation, catalog layout, typed value mapping |
| `src/can_reader.*`, `src/can_writer.*`, `src/can_socket.*` | Bounded controller transport and read/write state machines |
| `src/program_controller.cpp` | Controller-write helper used by the Qt application |
| `tests/` | Offline tests, SocketCAN test programs and private fixtures |
| `scripts/` | Build, launch, packaging, virtual CAN checks and fixture/catalog generation |
| `translations/rnet_de_DE.ts` | German Qt Linguist catalog; English source language |

Internal `rnet_*` library/helper names, the `RNet` Qt translation context, `.R-net` document extension and historical evidence filenames are retained for compatibility. They are not the displayed product name.

## SQLite catalog

`resources/dfreernet.sqlite3` is the pinned, versioned data source. It contains structured tables for parameters, parameter texts/fields, storage maps/groups/sections/items, editors/options, filenames, menus and resource texts. Opaque template/reference data remain in explicitly named BLOBs. Unsigned 64-bit storage keys are preserved as decimal text.

The build verifies the checksum and SQLite integrity, then compresses and embeds the database. Runtime opens a private temporary copy read-only through QtSql/QSQLITE and loads the catalog directly from the tables. The local application directory also includes an inspectable copy under `share/dfreernet/`. Editing that copy does not replace the embedded runtime catalog.

RND imports create new database files using a transaction and readback checks; existing files are rejected. Imported catalogs are not automatically activated for controller access. Developer table-export/database-generation tools are `scripts/export_catalog_tables.cpp` and `scripts/build_catalog_sqlite.py`; ordinary builds do not import an RND file.

## Preferences and translation

Application identity: `LocalRNet` / `dfreernet`, version 1.5.0. Qt QSettings stores preferences in the platform-native location (normally `$XDG_CONFIG_HOME/LocalRNet/dfreernet.conf` on Linux). On first use of the new name, existing `LocalRNet` / `R-net Qt` keys are copied only if the new store is empty. The old store is left intact. Other persisted paths remain intact; default application-local paths use the new application name.

English (US) is the source language; German uses a compiled Qt `.qm` resource. Add translated strings with the existing `RNet` context, preserve placeholders, update the `.ts` catalog and run translation tests. Qt standard widgets use the installed Qt German translations.

The title bar contains `dfreernet <version>` or `filename — dfreernet <version>`. The Qt application display name is short to avoid a second expanded product title being appended by the platform. The subtitle remains in documentation and the About dialog.

## Verification

GitHub Actions runs the fixture-free `scripts/test.sh` suite for both RELEASE and DEBUG in a Fedora 44 container. The job uses an offscreen Qt platform and does not access physical CAN hardware. Local private-fixture and virtual-CAN tests remain separate.

```bash
scripts/test.sh
# Shared heartbeat decoder check (offline):
QT_QPA_PLATFORM=offscreen build/RELEASE/rnet_preferences_test heartbeatConsumesSharedFrames
```

Do not point integration tests at a live CAN interface. Standard checks run offline. Release 1.5.0 registers 22 offline test targets without optional fixture-dependent targets. Additional cases are enabled when private fixtures are available. Visual changes should include Qt screenshots; tests already capture settings and startup layouts into `analysis/`. Physical access requires a stationary wheelchair and an explicitly authorized operation. Never infer a write authorization from a code change request.

## Local artifacts and recovery

Run `scripts/package.sh` to create `dist/dfreernet/bin` with the application and all required controller helpers, plus bilingual documentation. This directory is relocatable on Linux hosts with compatible Qt 6 including its QSQLITE driver, and zlib installed. It does not bundle those system libraries.

Build trees, Python caches, captures and checkpoint archives are generated/private and ignored. Keep source, fixtures and documentation separate from runtime captures. Do not delete original programmer resources or user configuration files during cleanup.

The local-only files `QUELLSTAND.sha256`, `LETZTE_SICHERUNG.txt` and `CHECKPOINT.json` record development checksums and recoverable checkpoints. They are excluded from Git together with captures, personal configurations and build outputs. Earlier local evidence remains available to the original developer.

Independently authored code is GPL-3.0-only within the scope stated in `SCOPE.md`. The standalone directory contains excluded derived catalog, text and template resources; see `resources/PROVENANCE.md` for origin and redistribution limits. The code license does not clear distribution of the complete bundled application.

## Sleep inhibition and heartbeat recovery

`SleepInhibitor` uses asynchronous QtDBus calls: a logind file descriptor for `sleep:idle` (fallback `sleep`) and a desktop inhibition cookie. Disabling/exiting releases handles; late replies after cancellation or owner destruction are cleaned up as well. Only the main window holds these locks; controller helpers do not. Tests use a private D-Bus and fake services to verify acquisition, release and late replies. Ordinary UI tests disable real sleep inhibitors.

The Qt process binds one SocketCAN raw socket. The window receives frames through it and forwards them to the heartbeat indicator, clock, distance dialog, live dump, and bounded monitor. A `CanSendQueue` thread waits for queued frames and writes them through that same socket. A mutex serializes reads, writes, interface checks, and detach before close. The window rebinds after interface loss or USB re-enumeration; the heartbeat indicator never opens a socket or transmits. The offline regression checks complete heartbeat frames, loss of the shared socket, and recovery. Controller helper executables run as separate processes and retain their own transport and interface lease.

The original programmer's distance-clear messages are extended CAN IDs `0x18300F05` (trip) and `0x18300F06` (total), both with **DLC 0**. In its `CCANMsg` structure, the payload length is at offset `+0x14`; the value `4` at `+0x18` is a message type. An earlier implementation sent DLC 4 and was followed by a reported PM system trip `8104` (unexpected receive DLC). The offline dialog test asserts both zero-length frames. Do not test a reset on physical hardware without explicit authorization and a stationary wheelchair.

Primary references: [systemd Inhibitor Locks](https://systemd.io/INHIBITOR_LOCKS/), [Idle Inhibition Service](https://specifications.freedesktop.org/idle-inhibit/0.1/index.html).

## Optional Wine programmer subproject

[`wine/README.md`](../wine/README.md) describes the separate PE32 `ftd2xx.dll`,
local USB-command processing and native UDP/SocketCAN proxy. Build with
`wine/build.sh`; `python3 wine/tests/offline.py` exercises the DLL under Wine
without opening CAN. `wine/run.sh --prepare-only` stages a private copy of the
installed OEM Programmer. Actual `wine/run.sh` operation forwards Programmer
CAN requests to `can0` and requires explicitly authorized stationary hardware
use. The native dfreernet application does not depend on this DLL.

## Linux attendant subproject

The Linux [`qt_attendant`](../qt_attendant/README.md) subproject starts live control on `can0` without options using a validated private profile. Its `run.sh` launcher configures both `can0` and `can1` at 125000 bit/s (override: `RNET_BITRATE`) before live startup; already correct UP adapters are left unchanged. Required changes respect the shared interface lease. Only permission-denied `ip link set` calls are retried using `sudo -n`, keeping the GUI unprivileged. Setup failure prevents application launch; completed setup steps are not rolled back. Offline/observe/playback/help/UI-check launches skip this setup. The setup tests simulate commands and never open real CAN. Direct binary launches require preconfigured interfaces.

Pure state machines separate registered startup, system-requested enrollment, table transfer, POP reads, readiness, ownership, mode changes and return. Only confirmed ownership enables joystick/speed output. Joystick travel is ±100 independently of speed (0..100). UI input is refreshed every 50 ms and expires after 250 ms without refresh; the 10-ms CAN timer cannot refresh stale input. Focus changes preserve protocol state, ownership, mode and speed, while immediately centering the joystick. The running QML UI emits neutral heartbeats in the background; returning to the window requires a new pointer gesture. Background UI failure still expires the input watchdog. Power-down latches until `00C`; interface checks/reconnects never restore an old ownership request. Close, SIGTERM and SIGINT request bounded return before Qt exits. One live app per interface and the shared controller-helper lease prevent competing senders. `--offline` opens no CAN socket; automated startup checks use offline mode or an invalid interface. Private capture tests skip when fixtures are missing. The latest default-start/shutdown hardening has offline coverage but still needs a fresh live validation. Build with `-DDFREERNET_BUILD_ATTENDANT=OFF` to omit it and its Quick dependencies.

## Native IR editor

[`qt_iredit`](../qt_iredit/README.md) edits local RnIR3 menus and Pronto Hex without Wine or CAN. The bounded native NRBF parser preserves menu structure and both signal slots; physical signal conversion remains experimental. The main build includes its offline parser and Qt Widgets checks.
