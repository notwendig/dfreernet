# Repository Guidelines

## Project Structure & Module Organization

`src/` contains the C++17 core for documents, catalog values, and CAN transport; `src/qt/` contains the Qt Widgets interface. `tests/` holds C++ and Python checks, while `scripts/` provides build, launch, packaging, and investigation tools. The pinned catalog and illustrations live in `resources/`; German translations are in `translations/rnet_de_DE.ts`. Use `docs/DEVELOPMENT.md` for architecture and verification details. `build-native/`, `dist/`, and `analysis/` are generated or local work areas.

## Build, Test, and Development Commands

On Linux, install CMake 3.21+, Ninja, a C++17 compiler, Python 3, zlib development files, and Qt 6.10+ with Widgets, Sql, DBus, LinguistTools, and Test.

- `scripts/build.sh` configures and builds `build-native/dfreernet` with Ninja.
- `scripts/run.sh` rebuilds if needed and launches the application; pass `--language en_US` for an English UI.
- `scripts/test.sh` builds, runs CTest with failure output, and checks runtime dependencies.
- `scripts/package.sh` creates a relocatable local application tree under `dist/dfreernet/`.

## Coding Style & Naming Conventions

Use C++17 and follow the surrounding file's indentation and brace style; existing files vary between two and four spaces. Keep source/header pairs such as `can_reader.cpp` and `can_reader.hpp`, and use lowercase snake_case for files and functions where practical. CMake enables `-Wall -Wextra -Wpedantic` for native targets. No repository-wide formatter or linter configuration is checked in. English is the source UI language; update the German `.ts` catalog when adding translated strings, preserving placeholders and the `RNet` context.

## Testing Guidelines

Put new C++ checks in `tests/*_test.cpp` and Python checks in `tests/test_*.py` or `tests/*_test.py`, then register applicable tests in `CMakeLists.txt`. Run `scripts/test.sh` before submitting. Standard checks are offline; some CTest cases register only when private `tests/fixtures/` files are present. Do not run integration tests against a live CAN interface. Include screenshots for visible Qt changes and document any fixture-dependent checks you could not run.

## Commit & Pull Request Guidelines

Recent commits use short, imperative subjects, such as `Filter profile parameters and strengthen startup warning`; releases use `Release dfreernet X.Y.Z ...`. Keep commits focused. In pull requests, describe behavior, verification commands and results, relevant issues, and screenshots for UI changes. Explain controller-read or write implications explicitly.

## Resources & Safety

Keep private captures, personal configurations, and controller backups out of Git. Check `SCOPE.md` and `resources/PROVENANCE.md` before distributing bundled resources. Physical controller operations require explicit authorization and a stationary wheelchair; a code change request alone does not authorize device writes.
