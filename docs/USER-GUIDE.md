# dfreernet — User guide

[English](USER-GUIDE.md) | [Deutsch](USER-GUIDE.de.md)

**Dongle free R-Net Programmer**, an experimental Qt 6 application for Linux.

**Rudimentarily tested on Permobil T5 VS.** The recorded trials do not establish general compatibility or replace complete validation. Other models and firmware versions are not confirmed.

## Requirements and startup

Linux with Qt 6.10 or later and a suitable SocketCAN interface. The application does not configure the CAN adapter. Interface, wiring and bitrate must match the actual controller. The development setup used `can0` at 125 kbit/s; this is not an automatic recommendation for other systems.

The verified catalog, UI texts and snapshot template are embedded in the application. The app uses its embedded resources and Linux SocketCAN. For a copied standalone directory, launch `bin/dfreernet` and keep its controller helpers beside it. Installed Qt 6.10+ including the Sql module/QSQLITE driver, and zlib are still required.

```bash
scripts/run.sh
```

The modal warning appears at every startup. “I understand — Continue” opens the interface; Exit and Escape end startup. On the first start, the window uses the second monitor when available. Later starts restore its last position, size and maximized state; the warning appears on the same screen. The Language menu switches between US English and German.

## Edit → Settings

Four vertical tabs group the persisted preferences:

| Tab | Contents |
| --- | --- |
| General | Language, default Dongle access level, undo limit |
| CAN | Interface, monitor duration, heartbeat visibility and reception timeout |
| Files and folders | Working, backup, export and log folders |
| Appearance | Details pane, icon and font sizes |

Apply accepts valid values; OK accepts and closes; Cancel discards edits that have not been applied. The log folder takes effect on the next start. Explicit command-line arguments take precedence. Controller operations use the saved CAN interface and backup folder without requesting them again. Safety confirmations remain; opening or saving a file still requires choosing its filename.

## Read configuration

Keep the wheelchair stationary; driving must not be needed during access. Select Read controller and confirm stationary operation. Reading can occupy programming access and temporarily block driving.

Received files are saved in a new unique subfolder of the configured backup directory. The interface prefers the configuration of verified repository node 2. Supported parameters appear as values; unknown file versions are retained as raw data and are not presented as known parameters.

## Edit parameters and save files

Under **Views**, choose **Hierarchical list**, **Profile table**, or **Module groups**. The profile tree offers Common and Profile 1–8. The module tree groups parameters by the first two parts of their catalog path. The right table shows the selected group. Profile selections show only parameters mapped to profile storage; applicable unread parameters are gray when enabled in Settings. Search, access filtering and CSV export work in all three views. The choice is remembered.

Column widths and order are saved automatically after changes and restored when you reopen a view or restart the app. **Views → Save column layout** saves immediately; **Load column layout** restores the last saved state, and **Reset column layout** returns to the defaults. Each table view has its own layout.

The profile table's Module / group column distinguishes identically named parameters belonging to different R-net modules. A suffix such as “/ 2” denotes the second element or channel.

The column immediately after the raw value combines unit, scale and raw minimum/maximum (`unit · a:b · min–max`). The column title uses two lines; cell values remain on one line. The tooltip labels each field. Unit comes from the catalog symbol and prefix; the numeric limits are raw catalog values. These metadata do not convert the stored raw value into engineering units. The colored access column remains fixed on the right during horizontal scrolling.

Use search and the group tree to narrow the display. Double-click or press F2 in the raw-value column to open the appropriate editor: boolean, selection, integer or text. Bounds, step size and text encoding are checked. Unresolved special types remain read-only. Escape discards the current edit.

Select a parameter and press **F1** (or **Help → Parameter help**) to see its embedded catalog help text together with the ID, group, profile, raw value and access level. The R-net PC Programmer guide describes this per-parameter help in section 3.3.2. The help window works without the original programmer installation.

Changes appear red and bold. Undo restores earlier values. Save file and Write file as save a working copy; this does not transfer values to the controller or confirm a controller write.

The access column shows 0 Therapist (yellow), 1 Dealer (green), 2 OEM (blue), 3 Factory (red). The Dongle menu limits display and permitted editing. This local selection does not replace manufacturer qualification or guarantee controller-granted access.

## Write to controller

The current implementation supports only verified direct parameter mappings and the tested target node. It is not a universal firmware flasher. Review the preview, keep the wheelchair stationary and retain a backup. The writer compares the current controller state, backs up data and requests readback afterwards.

An acknowledged commit without verified readback is **not a fully confirmed success**. Changes may already be active. Do not write again in that situation: read the controller and inspect actual values and logs. Multiple files are not an atomic transaction; a failure can leave a partially changed configuration.

## Import an RND catalog

Choose **File → Import RND catalog…**, then select an RND file. RND contains parameter definitions, help text, limits, access levels, selections and storage mappings. Actual wheelchair settings are stored in `.R-net` files.

The importer decrypts and validates the supported catalog format and asks for a new `.sqlite3` filename. Existing files are not overwritten. Parameters, text fields, metadata, storage mappings and editors are stored in structured SQLite tables; the resulting database is checked. The built-in catalog and controller data remain unchanged. Invalid or unsupported formats are rejected.

## Keep Linux awake while the app is open

Enable or disable **Edit → Settings → General → Prevent Linux sleep while the app is open**. It is enabled by default. The app requests a temporary system sleep inhibitor and, when supported by the desktop, an idle/screen-blanking inhibitor. “Linux awake” on the right of the status bar indicates an acquired system lock; its tooltip describes partial or unavailable support. Disabling the option or closing the app releases the locks. Persistent system power settings are unchanged.

This applies to the Linux computer. The wheelchair Sleep Timer is unchanged. A passive 30-minute test showed that lighting toggles do not keep the tested T5 VS awake. The light cycle never starts automatically.

## CAN heart and identification

The heart at the right of the status bar pulses red while recent complete `0x03C30F0F` device-heartbeat frames with seven `0x87` bytes are received. The `0x00E` serial-identification frame does not activate it. Without recent reception it is gray and still. A down interface is reported explicitly in the tooltip. The receiver rebinds automatically after USB/interface replacement; it never configures the adapter or sends CAN messages. The indicator only listens and sends no CAN messages. It does not confirm driving readiness or write permission. A gray heart may also indicate an incorrect interface or a different heartbeat on the connected system.

Serial queries and CAN monitoring use the saved interface. A serial number does not automatically establish a reliable module name.

The panel below the fixed access column continuously displays received CAN frames with reception time, interface, ID, DLC and payload. It starts with the app, reconnects when the selected interface returns, never transmits, and keeps at most 2000 display lines. **Controller → Monitor CAN…** remains a separate timed capture summary.

Drag the horizontal divider above the lower panes to change their height. The divider position is saved when the app closes.

## Troubleshooting

Check the interface and folders in Settings first. Retain backups, logs and the application version. Do not repeat an unconfirmed transfer before reading the actual state. A complete recovery procedure has not been established for every controller variant.

## Optional live signal: hazard lights

Under **Edit → Settings**, the CAN page provides the light-cycle switch, interval (default **300 seconds**) and pulse duration (default **60 seconds**). The CAN menu also starts or stops the cycle. It is **off at every app start**; a saved enabled value is ignored.

A current heartbeat is required. Before starting, the app asks you to confirm that the wheelchair is stationary and the hazard lights are **physically OFF**. The command only toggles them; there is no measured lamp state. Starting while the lights are on leaves them on between pulses. The status icon shows timing, not actual lamp state.

The first toggle is immediate, the second follows after the configured duration. If CAN reception or controller access becomes unavailable, the cycle stops and does not resume automatically. Closing or disabling attempts to complete a pending pair. If sending fails or someone else changes the lights, check them physically.

A passive 31-minute test on 11 November 2026 recorded 31 toggle pairs. The chair powered off exactly 30 minutes after the last joystick activity, only 37 seconds after the final pair. The cycle **does not prevent automatic power-off** on the tested T5 VS. No controller parameter is changed. CLI startup with `scripts/run.sh --hazard-blink-test` also requires the visible confirmation and a current heartbeat.

In **Appearance**, enable **Toolbar: icons only** to hide toolbar labels. Tooltips still name each action.

The current Dongle mode is always visible as a colored status badge, using the same yellow/green/blue/red colors as the permission column. OEM/Factory selection warnings remain.

## Parameter visibility and Dongle level

**General → Show all parameters up to the selected Dongle level** is independently configurable. Enabled (default): definitions without decoded values are shown as **Not read**, in gray and read-only. Disabled: only decoded values from the document/controller read are shown. Both modes respect the current Dongle ceiling. Gray entries are never invented values and cannot enter a controller-write plan.

The default level is **Dealer (1)**. Therapist (0) can also be selected as a saved default. OEM (2) and Factory (3) require a warning in the Dongle menu and apply only to the current app session. Their colors remain yellow, green, blue and red for levels 0–3. Changed loaded raw values remain red and bold.
