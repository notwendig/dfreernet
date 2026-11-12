# dfreernet

[English](README.md) | [Deutsch](README.de.md)

**Dongle free R-Net Programmer** · Native Linux application · C++17 / Qt 6 / SocketCAN

An experimental desktop application for reading R-Net controller configurations, editing supported parameters, and saving configuration files. It connects through Linux SocketCAN without a programmer dongle, Wine, or FTDI/CUSBMsg runtime DLLs. The verified parameter catalog, menu/text resources, cryptographic constants and configuration-document template are embedded in the native binaries. Build and runtime use only checked-in resources and installed Linux libraries.

**License:** Original code is [GPL-3.0-only](LICENSE). The bundled catalog and other third-party-derived materials are excluded; see the [license scope](SCOPE.md). A complete bundled application is not cleared for redistribution.

**Test status: rudimentarily tested on Permobil T5 VS.** This is limited development testing, not a general compatibility claim or manufacturer approval. No compatibility with other wheelchair models, controller variants or firmware versions is established. See [validation and limitations](docs/VALIDATION.md).

## Version 1.1.0

This version preserves column layouts when switching views quickly, rebuilds the launcher when sources change, and shows the version in the main title and startup warning. The Help menu is last on the menu bar. It remains experimental: controller compatibility and the write path are only partly verified. The repository excludes private controller fixtures, captures and backups; local reference tests require those fixtures.

## Quick start

Requires Linux, a C++17 compiler, CMake 3.21+, Ninja, Python 3, zlib development files, Qt 6.10+ Widgets/Sql/DBus/LinguistTools and Qt Test for the default test build. The development machine currently uses Qt 6.11.2. A configured SocketCAN adapter is needed for controller access.

```bash
scripts/build.sh
scripts/run.sh
# Open a saved configuration:
scripts/run.sh --document '/absolute/path/configuration.R-net'
# Run the offline test suite:
scripts/test.sh
```

The native executable is `build-native/dfreernet`. The launcher works from this checkout at any location. For a relocatable local application directory, run `scripts/package.sh`, then start `dist/dfreernet/bin/dfreernet`. Copy the complete directory, including its controller helper programs. The target machine needs Qt 6.10+ including the Sql module/QSQLITE driver, and zlib installed; no Wine or original programmer is needed.

A warning and disclaimer appears at every interactive start. Choose Continue deliberately to enter the application. The interface defaults to US English; German is available through Language or **Edit → Settings → General**. Existing preferences from the former “R-net Qt” application are copied once when no dfreernet preferences exist.

## Main functions

- Read controller configuration and preserve received payloads, including unsupported blocks.
- Show parameters supported by the installed catalog and the received file version; filter by access level.
- Switch under Views between the category list, profile view, and module groups. The profile view lists Common and Profiles 1–8; the module view groups parameters by their catalog module. All use the same read values, search, access filter and editors.
- All three view buttons are visible beside the search field.
- Column widths and order persist automatically for each table; Views also offers save, load and reset actions.
- The profile table identifies the source module/group, so parameters with the same name but different IDs remain distinguishable.
- One compact column after the raw value shows catalog unit, scale fields, and raw minimum/maximum. The colored access column stays visible on the right while scrolling horizontally. No unverified engineering-unit conversion is applied.
- Edit supported raw values using validated boolean, selection, number and text editors. Pending changes appear red and bold.
- Press F1 on a selected parameter to open its bundled R-net catalog help text with ID, profile, raw value and access context.
- Open and save `.R-net` files; export displayed information.
- File → Import RND catalog… validates an RND file and exports its tables to a new SQLite database. The built-in catalog is not switched automatically.
- Write supported parameter changes with backups, comparison and readback verification. This is a bounded experimental implementation.
- Query module identifiers and serial numbers, and monitor CAN traffic.
- Show a passive CAN heartbeat as a pulsating heart at the right of the status bar.
- Keep Linux awake while the app is open; optional session-only hazard-light experiment (default interval 300 s, duration 60 s; requires physical OFF-state confirmation and does not prevent wheelchair sleep).
- Store CAN, folder, language and appearance preferences in grouped vertical settings tabs.

## Documentation

| Document | Purpose |
| --- | --- |
| [User guide](docs/USER-GUIDE.md) | Setup and operating instructions (English / German) |
| [Validation and limitations](docs/VALIDATION.md) | Actual test scope, evidence and known gaps |
| [Development](docs/DEVELOPMENT.md) | Build, structure, translations and checks |
| [Changelog](CHANGELOG.md) | Current changes |

## Project layout

```text
src/             Native core, SocketCAN tools and Qt interface
translations/    Qt Linguist German catalog; English source strings
scripts/         Build, launch, verification and investigation tools
tests/           Automated tests; private reference fixtures stay local
resources/       Pinned SQLite catalog and resource provenance
docs/            Current bilingual documentation
analysis/        Private captures, reports and screenshots (generated)
checkpoints/     Recoverable source snapshots (local)
build-native/    Generated native build, including dfreernet
```

Historical investigation files, local checkpoints, controller captures, personal configurations and build outputs remain outside Git. The repository contains the native application sources and the pinned runtime catalog; the bundled derived data are excluded from the code license.

## Use and distribution

Independent experimental software; not an official Permobil or controller-manufacturer tool. Incorrect settings can disable a wheelchair or cause unintended movement. Read the application warning and the [user guide](docs/USER-GUIDE.md) before controller access. Retain an independent backup and verify changes by reading back.

The independently authored code is licensed under GPL-3.0-only within the stated [scope](SCOPE.md). Embedded data were derived once from locally verified reference resources; see [resource provenance](resources/PROVENANCE.md). Runtime independence does not establish redistribution rights for these data. Original software, catalogs, documents, captures and binaries are not licensed for redistribution by this project. The product and company names identify compatibility context only; no endorsement is implied.
