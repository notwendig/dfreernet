# Validation and known limitations

[English](VALIDATION.md) | [Deutsch](VALIDATION.de.md)

Status: 17 November 2026. Application: dfreernet 1.3.1, native Linux / Qt 6.

For 1.3.1, `scripts/test.sh` passed all 10 fixture-free offline test targets. Five targeted Qt cases passed for the clock and distance dialogs, zero-length clear frames, and the shared CAN socket. `QT_QPA_PLATFORM=offscreen build/RELEASE/dfreernet --version` reported 1.3.1. Dialog screenshots are in `docs/screens/`. No physical distance reset or application-driven clock write was performed for this release; their physical behavior remains unverified.

## Hardware scope

**Rudimentarily tested on Permobil T5 VS**, as identified by the user. Existing logs record development operations against the connected R-Net system; they do not independently certify the wheelchair model, hardware revision or firmware inventory. No structured full-system acceptance test, long-duration test or general compatibility campaign has been completed. No driving-performance validation is claimed.

| Area | Recorded evidence | Scope |
| --- | --- | --- |
| Configuration read | `analysis/pop-reply-readback-result.json` and associated snapshot | 23 files / 3,581 bytes; 23 exact wire-payload comparisons on the connected development system |
| OBP parameter | Prior PID 68 change from 0 to 1 and readback; user reported OBP working | One specific authorized parameter change |
| Text changes | `analysis/write-readback-fix-findings.md`, `analysis/pop-reply-fix-findings.md` | User-initiated profile/axis-name changes; subsequent readback confirmed values without repeating writes |
| Serial identification | `analysis/real-can-serial-findings.md` | Responses and original formatting inspected; module-type naming remains incomplete |
| Settings and heartbeat | `analysis/settings-heartbeat-tests.log`, `analysis/settings-heartbeat-vcan.log` | Persistence, EN/DE UI, filtering, timeout, zero indicator transmissions in private vcan test |
| Live signal and unread definitions | `analysis/live-signal-settings-tests.log`, `analysis/live-signal-settings-vcan.log` | 17/17 offline targets; saved enable/interval/duration, no sending before heartbeat, read-only gray definitions, role filtering and session-only elevated-level warnings; virtual CAN pair only |

These local evidence paths are private development artifacts, not a redistributable test data package. Later branding and documentation changes do not constitute additional physical hardware testing.

A later 31-minute passive CAN observation on 11 November 2026 recorded 31 hazard-light toggle pairs. The chair powered off 1800.328 seconds after the last non-neutral joystick frame, despite a pair ending 37 seconds earlier. Periodic module heartbeats continued until shutdown. The light cycle did not inhibit automatic power-off in that session. Raw data remain in the private local analysis folder.


A separate power-on comparison on 11 November 2026: one explicitly authorized `0x00C` frame briefly lit the display; no CAN module responded in the following six seconds, and the display went off again. On the subsequent physical On/Off-button start, the display stayed on. The passive capture began with `0x00C`, followed about 21 ms later by an eight-byte `0x00E`, then discovery frames and cyclic traffic. Thus `0x00C` alone is not a verified remote power-on method. Raw data containing a module identifier remain private under `analysis/`. A later single attempt with `0x00C` followed 23 ms later by `0x00E` carrying the previously observed identifier again caused only a brief display flash; no CAN response arrived within eight seconds. This pair is likewise not a verified remote power-on method. Three subsequent `0x00C` frames spaced 250 ms apart again caused only a brief display flash without any CAN response; they did not keep the chair powered on. In a requested four-frame test using the `0x00E` serial-identification frame, incorrectly taken as the requested heartbeat, two `0x7B3` frames appeared after the second heartbeat; the test therefore stopped before sending the remaining two. The display briefly lit and went off again. The source of those frames and the reason the startup did not continue remain unverified.

A user-provided 23.652-second `can1` capture shows the on/off phases: `0x00C` at the start, an `0x00E` serial-identification frame 21 ms later, `0x7B1`/`0x7B0` during initialization, and the cyclic `0x03C30F0F` device heartbeat from 0.513 s. From 12.601 s, `0x002`/`0x004` mark the shutdown prelude; `0x002` RTR starts at 14.260 s, and `0x000` plus RTR close the trace. The capture has no sender-direction or physical-button markers, so phase labels are inferred from frame order. A subsequently requested single test on `can1` with four `0x03C30F0F` device frames, `0x00C`, and four more device frames produced only nine local CAN echoes and no response; the display briefly lit and went off again.

A passive `can0` observation on 15 November 2026 found extended frame `0x1C2C0000` once per second with six data bytes. Twelve successive payloads ran from `28 16 04 EF 0B 1A` to `33 16 04 EF 0B 1A`: byte 0 advanced by one each second. The plausible clock layout is seconds, minutes, hours, `(weekday << 5) | day`, month, and year since 2000; `0xEF` gives weekday 7 and day 15, matching Sunday 15 November 2026. The [Open-RNet frame dictionary](https://github.com/redragonx/open-rnet/blob/main/reference/RNET_FRAME_DICTIONARY.md) independently labels the `1C2C0X00` family “Time of Day,” without specifying these byte fields. The displayed CJSM2 time and sender were not independently confirmed in this first observation. No request frame was needed to observe it. Open-RNet names programmer `ReadDateTime` and `SendDateTime` operations but gives no CAN message for them.

In a later passive `can0` capture the user adjusted the clock via CJSM2 on-board programming. Two extended `0x1C2C0001` frames carried `00 2E 15 AF 0B 1A` and `00 2E 15 4F 0B 1A`; 387 and 405 ms later respectively, `0x1C2C0000` broadcast the same six bytes and then resumed incrementing the seconds. The user confirmed the display showed 21:47 afterward, matching hour/minute bytes `15 2E` in hexadecimal. This strongly identifies `0x1C2C0001` as the on-board clock-setting frame. The high three bits of byte 3 changed from 5 to 2 while day 15 stayed fixed, so the decoder must not require a weekday/date match. The raw capture remains private. No frame was sent by dfreernet; acceptance of an externally injected frame was unverified at this stage.

After the user explicitly confirmed stationary operation and authorized one physical send, a separate one-shot tool observed `0x1C2C0000#180204EF0B1A`, equal to the computer's current local time, sent exactly one `0x1C2C0001#180204EF0B1A`, and received the matching time broadcast. There was no retry. Because the clock already matched before sending, this confirms the desired clock state and the single transmission, but does not establish that an external set frame can change the physical RTC. The dfreernet UI itself was tested only on isolated virtual CAN.

In a subsequent test on 15 November 2026, the user explicitly requested an intentionally different value, confirmed the wheelchair was stationary, and authorized exactly one physical frame. The prior broadcast represented 15 November 2026 at 04:40:18–23. A single `can0` transmission of `0x1C2C0001#0000005A0A4D` encoded Tuesday 26 October 2077 at 00:00:00. A later passive readback showed `0x1C2C0000#2000005A0A4D` through `#2300005A0A4D`, advancing once per second at 00:00:32–35 on the requested date. This confirms that an externally sent frame changed the physical R-Net clock. The send used `cansend`; the dfreernet UI has still been tested only on isolated virtual CAN. No retry or second set frame was sent.

## Automated checks

`scripts/test.sh` builds the application, runs the offline CTest suite, verifies pinned embedded-resource checksums during the build and checks native runtime dependencies. The local suite with private fixtures contains 17 targets; the Git snapshot runs the fixture-free subset. It covers document preservation, catalog decoding, value validation, bounded write logic, capture audits, translations, startup consent, preferences and Qt interaction. These tests do not send to a physical CAN bus.

The optional heartbeat integration test uses a fresh isolated network namespace and virtual CAN interface. It verifies that unrelated and incomplete frames do not activate the indicator, a complete extended `0x03C30F0F` frame with seven `0x87` bytes does, stale reception returns gray, and the indicator sends zero frames. It requires Linux namespace/vcan support and is separate from standard CTest.

For 1.2.0, all eight fixture-free offline tests passed. A separate isolated-vcan test confirmed that the continuous CAN dump and timed monitor both received the test frame while the application transmitted zero frames. No physical-controller test was performed for this release.

For 1.2.1, `scripts/test.sh` passed all eight fixture-free offline tests. `unshare --user --map-root-user --net python3 scripts/test_qt_can.py` passed the private-vcan follow/count and passive system-test checks; the application transmitted zero CAN frames. No physical-controller test was performed for this release.

For 1.2.2, the RELEASE build passed all eight fixture-free offline tests. The DEBUG build passed seven in the first run; the remaining DBus sleep-inhibitor test passed when rerun outside the sandbox after a local socket bind was denied. The isolated vcan test passed the icon-controlled Follow/Count and Hz/ms views in the live dump and System Tests; the application transmitted zero CAN frames. The Qt tests also checked CAN table layout, view selection, and window geometry restoration. No physical-controller test was performed for this release.

For 1.3.0, `scripts/test.sh` passed all eight fixture-free offline tests. Eight targeted Qt test cases passed for CAN counts, sorting, layout restoration, and the R-Net date/time status display. `unshare --user --map-root-user --net python3 scripts/test_rnet_clock_vcan.py` passed with exactly one app-generated clock-set frame and a matching readback in isolated virtual CAN. `dfreernet --version` reported 1.3.0. The physical clock change described above used `cansend`, not the application UI.

The later clock change passed the eight fixture-free offline tests and the targeted Qt status-widget test. `unshare --user --map-root-user --net python3 scripts/test_rnet_clock_vcan.py` verified one app-generated `0x1C2C0001` frame with host date/time bytes and a matching `0x1C2C0000` readback in an isolated virtual CAN network. The network recorded exactly three transmissions: the test's initial time frame, the app's single set frame, and the test's readback frame. No app-generated clock frame was sent to physical hardware.

## Known gaps

- Complete OEM application reconstruction is unfinished. Unsupported actions remain unavailable.
- Additional repository file versions and parameter conversions need verified mappings. Unknown payloads are preserved, not guessed.
- Encrypted document metadata generation remains incomplete; configuration snapshots may retain metadata from the original default template. Current module serials must be considered separately.
- The writer is bounded to supported mappings and the verified node-2 repository path. Other target modules and arbitrary firmware are not supported by inference.
- A multi-file write can be partially applied. Commit acknowledgement and verified readback are distinct states; no automatic write retry is performed.
- Reliable module-name/type mapping is incomplete.
- The passive heart watches the observed seven-byte `0x03C30F0F` device heartbeat. This is not proof of whole-system health or universal heartbeat coverage.
- The `0x1C2C0000` clock layout and `0x1C2C0001` on-board set frame were derived from passive observations. A later single external physical send changed the RTC to a deliberately different date and time, confirmed by the broadcast readback. The dfreernet UI itself remains untested on physical hardware; an active read request is also unknown.
- The native app embeds derived reference resources. Their provenance and redistribution restrictions remain; this is not an independently authored replacement catalog. Opaque document metadata still come from the bundled reference template.

## Reporting a reproducible issue

Record application version and source checksum, Linux/Qt versions, adapter and interface configuration, exact controller/module firmware if known, operation and expected/actual result. Preserve the before/desired/readback files and task log. Redact personal and serial data before sharing. Do not repeat an unverified write merely to reproduce a report.
