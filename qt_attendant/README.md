# R-net attendant control for Linux

[English](README.md) | [Deutsch](README.de.md)

Part of dfreernet 1.5.0.

Qt 6 / C++17 application for the captured and tested R-net configuration: attendant slot 0, passenger slot 1, PM slot 3 and attendant profile 8. Mode 1 is driving; mode 2 is seating. A private profile supplies device identities, expected peers and startup tables.

## Start without options

```sh
qt_attendant/run.sh
```

Or, after building:

```sh
build/RELEASE/qt_attendant
```

Open the application **before powering on the wheelchair**. Control on `can0` is the default. `run.sh` configures **can0 and can1 at 125,000 bit/s** and brings both UP before launching the app. Adapters already UP at this bitrate are left unchanged. Direct binary launches require interfaces to be configured beforehand. Follow the [connection and galvanic-isolation guide](../README.md). Disconnect the original attendant device when using the emulator.

Setup requires `ip` (iproute2) and Python 3. When network permissions are missing, the script retries only the required `ip link set` commands through `sudo -n`; the application itself runs as the normal user. If sudo requires a password, run `sudo -v` in the same terminal, then retry `qt_attendant/run.sh`. Both adapters must be present. Missing adapters, a held interface lease or setup errors prevent application startup. Completed setup steps remain in effect if a later step fails. Set `RNET_BITRATE` for another known bus bitrate; the default is `125000`. The application still uses only `can0` by default; `can1` is configured but not used by it.

The app first sends its normal identity burst. If the system recognizes the registered BGM, normal startup follows. If the system explicitly requests initial enrollment, the app answers after validating the known peer set. It transmits only its own slot-0 table records and requires matching peer responses at each stage. After initial enrollment, it indicates the required restart. Keep the app open for the next power-on; it tries normal startup again. Rejection and timeout do not cause an enrollment retry loop.

Takeover requires confirmed identity, configuration and module readiness. Press **Take control** with the joystick centered. Mode changes and return wait for confirmed suspension before selecting the next mode/profile. On return, the physical passenger device selects its own mode; the app waits for the complete confirmation sequence.

## Controls

Three buttons accompany the joystick:

- **Take control / Return to passenger:** centered takeover; returning also works while displaced and immediately sends neutral.
- **Profile:** switches the two modes within fixed attendant profile 8. The displayed mode changes only after confirmation.
- **Speed:** cycles through 0, 20, 40, 60, 80 and 100%. The initial setting is 20%.

Joystick travel and speed are independent. Joystick axes reach ±100 in every mode; speed is sent separately via `0A040000`. Radial bounds, the neutral zone and enable checks remain enforced.

![Offline interface, German UI](../docs/screens/attendant-safe-de.png)

The app stays open when the wheelchair powers down. A new `00C` begins the next startup exchange. Window close, Ctrl+C and SIGTERM request confirmed return before exiting. This handshake is bounded; missing confirmation stops output and is logged. An uncatchable process termination cannot execute a CAN exchange.

## Failure handling

- No takeover before confirmed module readiness and control acknowledgement.
- No joystick/speed output while a mode selection is pending or after return.
- Fresh UI input is required; its absence causes neutral and return after 250 ms. A held pointer/touch input is refreshed by the running UI every 50 ms.
- Focus changes preserve registration, ownership, profile/mode and speed. Joystick displacement returns to neutral. The running UI refreshes only neutral in the background; a new gesture is required to move after returning. Missing fresh UI updates still trigger the 250-ms watchdog and return.
- `002` RTR is the power-down barrier: no further output until the next power-on marker.
- Interface loss, send failures, conflicting identities and missing protocol acknowledgements stop the affected exchange. Receive connection retries are bounded in frequency; control requires another power-on cycle.
- One live app instance per interface. The sender also uses the same interface lease as dfreernet's controller helpers.

The protocol state machines target the known device arrangement. Earlier stationary-wheelchair tests confirmed enrollment/restart, takeover/return, direct mode changes, seating adjustment and the lowest speed setting. The new automatic startup selection and additional failure/termination paths were subsequently checked **offline**; a combined live retest remains outstanding.

## Private profile

Without options, the app first looks for `enrollment.json` in Qt's application configuration directory, typically `~/.config/LocalRNet/qt_attendant/` on Linux. Otherwise the local build uses `build/attendant-enrollment-tables.json`. The existing local profile is sufficient here for starting without options.

The profile holds identity, challenge keys, four expected peers, full device identities and captured tables. Missing or invalid profiles abort before opening CAN. The old separate identity file is no longer needed.

Generate a profile from private original captures:

```sh
python3 qt_attendant/tools/build_enrollment_profile.py insys.txt startup.log \
  --output build/attendant-enrollment-tables.json
```

`startup.log` must contain a complete matching identity burst. The tool refuses to overwrite a different existing profile. Profiles, original captures, configurations and local analyses stay out of Git and are not installed with the application.

## Diagnostics and offline verification

```sh
qt_attendant/run.sh --offline
qt_attendant/run.sh --observe
qt_attendant/run.sh --play-capture capture.log
```

`run.sh` skips network setup for `--offline`, `--observe`, playback, help/version queries and automated UI checks. For `--observe`, the adapter must already be UP.

`--offline` opens no CAN interface and enables local simulation. `--observe` receives only and disables controls. Playback is entirely offline. Other diagnostic options include `--registration-only`, `--interface NAME`, `--enrollment-profile FILE` and `--language de_DE|en_US`. `--initial-enrollment` forces initial enrollment for one power-on only; normal startup determines the need from the observed system exchange. The old `--can-tx` and `--experimental-enrollment` gates are no longer required.

```sh
scripts/test.sh
```

Tests simulate `ip`/`sudo` commands to check can0/can1 setup and launch ordering. Other tests use pure state machines, saved captures, offline UI or a deliberately invalid interface name. There are no integration tests against a live wheelchair. Private capture checks skip when their files are unavailable. The subproject needs Qt Quick, Quick Controls 2, Qt Test and QuickTest in addition to the [main development dependencies](../docs/DEVELOPMENT.md).

Passive capture remains available:

```sh
qt_attendant/tools/capture.sh startup can0 60
```

Logs stay in ignored `build/attendant-captures/`. This command starts no transmitter.
