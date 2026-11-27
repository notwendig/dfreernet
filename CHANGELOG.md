# Changelog

[English](CHANGELOG.md) | [Deutsch](CHANGELOG.de.md)

## 1.5.1 — 2026-11-27

- State that the original BGM must be disconnected from the R-net bus before starting the emulator and remain disconnected throughout operation; both use the same captured module identity/slot.
- Use “R-Net Control” / “R-Net Steuerung” as window, tray and launcher title. Share a joystick app icon across window, taskbar and tray, and supply the matching SVG desktop icon. Record the user's confirmation of registration, deregistration, reconnect and control on the local system.
- Add bounded verification/reactivation of an already registered BGM when the app starts on running CAN: match live identities/slot assignment and captured configuration readbacks; keep initial registration, readiness resets and automatic ownership changes out of this path. Add private reference extraction, cleanup and offline rejection/replay tests. The user has confirmed reconnect and control on the existing system.
- Make ordinary BGM window closing hide to tray; keep full GUI termination in the tray context menu. Suspend control and hide after confirmed return, restore on resume without taking control, and handle tray availability delayed at login. Add a user autostart installer for the existing binary and offline close/pause/startup regression checks.
- Match the BGM power-on lifecycle: use the common `00C` exchange for discovery; profiles without a reconnect reference wait for power-on instead of sending enrollment bursts into normal operation. Show an explicit restart instruction, distinguish initial observer cleanup from a real disconnect, and retain module participation while tray control is suspended. Add offline regression checks.
- Add optional tray startup/minimization with saved preferences, visible recovery when the tray is unavailable, and a tray control-suspension action. Suspension returns ownership without disconnecting the registered BGM; resuming requires a new explicit takeover. Retain orderly return before quitting, including hidden-window exits. Add offline tray, suspension and hidden-window heartbeat checks.

## 1.5.0 — 2026-11-27

- Add the Linux `qt_attendant` R-net attendant emulator with confirmed registration/readiness, takeover and return, driving/seating modes within profile 8, full-range joystick axes and independent 0–100% speed control for the captured device arrangement.
- Start without options using a validated private enrollment profile. Configure `can0` and `can1` at 125000 bit/s in the launcher; leave correct UP interfaces unchanged, respect sender leases and keep offline/diagnostic starts free of network changes.
- Preserve registration, ownership, mode and speed across focus changes while centering the joystick. Require a new gesture after returning; retain the independent input watchdog, power-down barrier and confirmation timeouts. Window close, SIGINT and SIGTERM request confirmed return before exiting.
- Add the standalone `qt_iredit` with English/German UI, native RnIR3/JSON editing, Pronto Hex validation, two signal slots, undo/redo and tree collapse/expand. Preserve existing menu data; R-net signal conversion remains experimental.
- Document both subprojects in English/German, add offline protocol/GUI/launcher checks and Qt Quick CI dependencies, and exclude private enrollment profiles and original captures. Latest startup/shutdown/focus and adapter-setup changes have offline coverage; a combined live retest remains outstanding.

## 1.4.2 — 2026-11-23

- Explain how to connect a Linux computer or Raspberry Pi through SocketCAN hardware, with a signal diagram, interface setup, reception checks and troubleshooting. Require galvanic isolation of CAN signals and CAN-side power; keep connector pin positions explicitly unverified.
- Add system timers, power-module logs and diagnostic views, with offline Qt checks.
- Add graphical OEM Programmer and IRConfigurator launchers with a shared launch lock, runtime staging and per-application logs.
- Extend the known Wine DLL and OEM/Factory EXE download deadlines in separate, SHA256-verified copies; preserve upload deadlines and installed originals. Include offline patch guards and document the delayed-acknowledgement replay results.
- Stage available MinGW runtime dependencies for the original IRConfigurator. Proprietary binaries, personal configurations and captures remain excluded; these changes do not establish general controller or Raspberry Pi compatibility.

## 1.4.1 — 2026-11-22

- Add `--factory-exe` to create a separate Factory-capable copy of the supported OEM Programmer, guarded by source and output SHA256 checks. Preserve the original and refuse unknown output files.
- Raise the application access limit from 2 to 3 and select the classic MFC file dialog for Wine compatibility; keep `DONGLE=Dealer|OEM|Factory` selection independent.
- Add offline patch guards and an extracted-instruction access-limit test. Verify Generic Factory, file promotion, Save As and reopening a private file in an isolated Wine profile with UDP replay; no physical CAN access or controller writes. Proprietary binaries remain excluded.

## 1.4.0 — 2026-11-22

- Add an optional PE32 FTD2XX DLL and native UDP/SocketCAN proxy for an existing OEM Programmer installation under Wine; process CUSBMsg locally and forward CCANMsg in both directions.
- Preserve extended CAN identifiers, RTR flags, zero-length frames, fragmented writes and three-frame bursts; retain the sticky Power-Off transmit gate.
- Support `DONGLE=Dealer|OEM|Factory` with matching EEPROM CRCs, OEM as default, and rejection of invalid profiles before transport startup.
- Let the original Programmer discover its repository node instead of fixing initialization to node 0; stage the runtime separately and keep per-run logs.
- Add offline DLL/transport/profile checks and optional crash-context logging. Proprietary binaries and Programmer EXE patches are excluded.

## 1.3.1 — 2026-11-17

- Refresh the English and German screenshots for the current UI; expand the About dialog so its text is fully visible and keep the R-Net date readable in the status bar.
- Add a Qt Designer clock dialog with live host and R-Net time and date, a host-to-R-Net action, and an editable R-Net target; use 24-hour clocks in both languages.
- Add Tools → Distance with live total and trip values and original zero-length CAN commands to clear either counter. Show whether the reset is confirmed by a matching readback; arbitrary distance values cannot be set.
- Share one application CAN socket across the live display, clock, distance, heartbeat, and monitor; serialize outgoing frames through a dedicated send-queue thread.
- Remove redundant stationary-wheelchair prompts from the clock and distance tools. Controller configuration writes still require their own authorization and stationary confirmation.

## 1.3.0 — 2026-11-15

- Show fresh R-Net Time of Day frames in the status bar and allow confirmed, single-frame synchronization to computer time with CAN readback.
- Show the R-Net date alongside the time. Color the date green when it matches the host and red otherwise; color the time green, orange, or red according to its absolute offset from the host.
- Add optional CAN interface setup at startup with a configurable bitrate, after the startup warning is accepted.
- Show CAN counters right-aligned with spaces instead of leading zeroes.
- Sort CAN table columns in either direction by clicking their headers; keep the order while frames arrive and after restart.

## 1.2.2 — 2026-11-14

- Share one CAN count table implementation between the live dump and passive System Tests.
- Add eight-digit counters, average rate in Hz or interval in ms, and icon controls for Follow/Count and Hz/ms in both CAN areas.
- Save and restore each CAN table's column widths and order, both view selections, System Tests geometry, and the group-tree divider.

## 1.2.1 — 2026-11-13

- Keep the continuous CAN dump beside details and task logs, with independently adjustable pane sizes.
- Add follow and grouped count modes to the receive-only CAN dump.
- Add passive live CAN views for all ten Tools → System Tests entries.
- Make the CAN dump pane and CAN table columns adjustable; save the pane width.

## 1.2.0 — 2026-11-13

- Remove the preferred-monitor and start-maximized settings; restore the main window's saved position, size and maximized state, and open the warning on the same screen.
- Make the lower details and CAN dump panes height-adjustable with a visible divider and save its position.
- Add a continuous passive CAN dump below the pinned access column, with automatic reconnection and bounded display history.
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
