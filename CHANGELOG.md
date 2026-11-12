# Changelog

[English](CHANGELOG.md) | [Deutsch](CHANGELOG.de.md)

## Unreleased

- Strengthen the bilingual startup warning about injury and possible destruction of motors or control electronics; add an illustrated sick wheelchair.
- Restrict each profile's table to profile-mapped parameters; keep applicable unread values gray.

## 1.1.0 — 2026-11-12

- Keep pending column layouts for every view while switching views quickly.
- Rebuild the launcher executable when source files are newer, avoiding a stale UI after updates.
- Replace deprecated Qt test assertions and resolve the GCC catalog-loader indentation warning.
- Show the version in the main title and startup warning; place Help last on the menu bar.

## 1.0.0 — 2026-11-12

- Save, load and reset column layouts separately for every table view.
- Show the author, contact email, current version and GitHub project link in About.
- Persist column resizing and reordering automatically, including when the app closes before the save delay.

## 0.2.2 — 2026-11-12

- Add a third parameter view grouped by catalog module.
- Combine catalog unit, scale and raw limits after the value; pin the colored access column at the right.
- Wrap long column titles across two lines.

## 0.2.1 — 2026-11-12

- Add F1 context help for the selected parameter using the bundled catalog text described by the R-net PC Programmer guide.
- Show module/group provenance in the profile table for identically named parameters with different IDs.
- Show catalog unit, scale pair, and raw numeric limits in the profile table without applying an unverified conversion.

## 0.2.0 — 2026-11-12

- Add switchable category and profile parameter views; the profile tree selects Common or Profile 1–8, with only that profile's values in the table.

- Require visible hazard-OFF confirmation, stop on lost CAN reception, and never resume a saved light cycle at startup. A 31-minute passive test showed the toggles do not prevent wheelchair sleep.

- License independently authored code under GPL-3.0-only; explicitly exclude the derived catalog and other third-party material.

## 0.1.0 — 2026-11-11

- First private Git source snapshot; align the application version and bilingual documentation.
- Use seconds for hazard-light interval and duration settings, show the light state as an icon, and support an icons-only toolbar preference.
- Keep personal controller fixtures, captures, checkpoints and generated builds outside Git.

### Earlier development work


- Add a permanent Dongle status badge using permission colors. Remove the separate light-test warning and confirmation; resume due lighting immediately when heartbeat returns instead of waiting another interval. Report missing CAN reception in status.
- Make the live hazard-light signal configurable (enable, interval, duration; defaults 300/60 seconds) with immediate first cycle and a new-session confirmation. Add optional gray unread parameter definitions, Dealer default and session-only OEM/Factory warnings. The observed cycle did not reliably prevent wheelchair sleep.
- Add an explicitly started five-minute hazard-light test, off at startup, with paired toggles, no-heartbeat/busy guards, stop-on-exit and no parameter changes; keep-awake efficacy remains unverified.
- Recover passive heartbeat reception after USB/interface replacement, report a down interface, and add a configurable Linux sleep inhibitor scoped to app lifetime.
- Store catalog and resource tables in SQLite; add File → Import RND catalog… with validated transaction-based export to a new SQLite database.
- Embed verified project resources for native startup, menus, catalog, settings and controller helpers; provide a relocatable local application directory.
- Rename the native application and executable to **dfreernet — Dongle free R-Net Programmer**.
- Use a short, unique window title: `dfreernet` or `filename — dfreernet`.
- Provide README, user guide, development, validation and changelog in English and German.
- Preserve previous Qt application preferences on first start under the new name.
- Replace the chronological root README with current documentation; retain previous notes under `docs/history/`.
- Document setup, operating flow, development, test evidence and known limitations, including rudimentary testing on **Permobil T5 VS**.
- Group persisted preferences in vertical tabs; add a passive pulsating CAN-heartbeat indicator at the right of the status bar.
- Retain startup warning, typed raw-value editors, access-level display, backups and bounded write/readback verification from prior development.

This is a development version, not a validated general release.
