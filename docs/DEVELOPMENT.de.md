# Entwicklung

[English](DEVELOPMENT.md) | [Deutsch](DEVELOPMENT.de.md)

## Bauen und starten

Die unterstützte Entwicklungsoberfläche ist eine native Linux-Anwendung mit C++17, CMake ab 3.21, Ninja, Python 3, zlib-Entwicklungsdateien und Qt ab 6.10 (Widgets, Sql, DBus, LinguistTools, Quick und QuickControls2 sowie Test und QuickTest für Tests). Im Betrieb werden keine zusätzlichen Netzwerkdienste benötigt. Auf dem aktuellen Entwicklungsrechner ist Qt 6.11.2 installiert.

```bash
scripts/build.sh
scripts/test.sh
scripts/run.sh --language de_DE
```

CMake-Ziel und Programmdateien: `dfreernet`, `build/RELEASE/dfreernet` und `build/DEBUG/dfreernet`. `scripts/build.sh` verwendet standardmäßig RELEASE (`RelWithDebInfo`); `scripts/build.sh DEBUG` verwendet CMake `Debug`. Mit `DFREERNET_BUILD_MODE=DEBUG` nutzen Start-, Test-, Paket- und Python-Integrationsskripte den Debug-Build. Die Skripte verwenden zwei parallele Buildaufträge. Compilerwarnungen sind für native Quelltextziele aktiviert. Bei manuellem CMake-Aufruf dieselben Qt-Module und ein separates Buildverzeichnis verwenden.

In VSCodium das Fenster neu laden und im Projektstatus von CMake Tools das Configure-Preset **DEBUG** oder **RELEASE** wählen. „Build“, „Test“, „Debug“ und „Launch“ verwenden dann das entsprechende `build/`-Verzeichnis.

Native Laufzeitressourcen werden aus geprüften Projektdateien unter `resources/` eingebettet. CMake startet `scripts/embed_resources.py`, das deren feste SHA256-Werte prüft und eine C++-Datei erzeugt. Das Hauptprogramm verlangt die ausdrückliche Zustimmung zur Startwarnung, bevor es die Dokument-/Controlleroberfläche erzeugt. `--quit-after` darf die Warnung für Oberflächenprüfungen ablehnen, aber nicht bestätigen.

## Quelltextübersicht

| Ort | Aufgabe |
| --- | --- |
| `src/qt/` | Widgets, Übersetzungen, Einstellungen, typisierte Editoren, Icons, Startwarnung, passiver Heartbeat |
| `src/document.*`, `src/database.*`, `src/values.*` | Dateierhaltung, Kataloglayout, typisierte Wertezuordnung |
| `src/can_reader.*`, `src/can_writer.*`, `src/can_socket.*` | Begrenzter Controllertransport und Lese-/Schreibzustandsautomaten |
| `src/program_controller.cpp` | Controller-Schreibhilfe der Qt-Anwendung |
| `tests/` | Offline-Tests, SocketCAN-Testprogramme und private Referenzdaten |
| `scripts/` | Build, Start, Paketierung, virtuelle CAN-Prüfungen und Fixture-/Katalogerzeugung |
| `translations/rnet_de_DE.ts` | Deutscher Qt-Linguist-Katalog; englische Quellsprache |

Interne `rnet_*`-Bibliotheks-/Hilfsprogrammnamen, der Qt-Übersetzungskontext `RNet`, die Endung `.R-net` und historische Nachweisdateinamen bleiben aus Kompatibilitätsgründen erhalten. Sie sind nicht der angezeigte Produktname.

## SQLite-Katalog

`resources/dfreernet.sqlite3` ist die geprüfte, versionierte Datenquelle. Sie enthält strukturierte Tabellen für Parameter, Parametertexte/-felder, Speicherzuordnungen/-gruppen/-abschnitte/-elemente, Editoren und Optionen, Dateinamen, Menüs und Ressourcentexte. Opake Vorlagen-/Referenzdaten bleiben als ausdrücklich benannte BLOBs erhalten. 64-Bit-Speicherschlüssel werden verlustfrei als Dezimaltext gespeichert.

Beim Build werden Prüfsumme und SQLite-Integrität geprüft und die Datenbank komprimiert eingebettet. Zur Laufzeit wird eine private temporäre Kopie ausschließlich lesend über QtSql/QSQLITE geöffnet. Der Katalog wird direkt aus den Tabellen geladen. Das lokale Programmverzeichnis enthält außerdem eine prüfbare Datenbankkopie unter `share/dfreernet/`. Änderungen an dieser Kopie ersetzen den eingebetteten Laufzeitkatalog nicht.

RND-Importe erzeugen neue Datenbankdateien in einer Transaktion mit Rückprüfung; vorhandene Dateien werden abgewiesen. Der importierte Katalog wird nicht automatisch für Controllerzugriffe aktiviert. Entwicklungswerkzeuge für Tabellenexport und Datenbankerzeugung liegen unter `scripts/export_catalog_tables.cpp` und `scripts/build_catalog_sqlite.py`; normale Builds importieren keine RND-Datei.

## Einstellungen und Übersetzung

Anwendungsidentität: `LocalRNet` / `dfreernet`, Version 1.5.1. Qt QSettings speichert Einstellungen am plattformüblichen Ort (unter Linux normalerweise `$XDG_CONFIG_HOME/LocalRNet/dfreernet.conf`). Beim ersten Start unter dem neuen Namen werden Schlüssel aus `LocalRNet` / `R-net Qt` nur übernommen, wenn der neue Speicher leer ist. Der alte Speicher bleibt erhalten. Andere gespeicherte Pfade bleiben bestehen; neue Standardpfade für Anwendungsdaten verwenden den neuen Namen.

US-Englisch ist die Quellsprache; Deutsch verwendet eine kompilierte Qt-`.qm`-Ressource. Übersetzungstexte im vorhandenen Kontext `RNet` ergänzen, Platzhalter erhalten, den `.ts`-Katalog aktualisieren und Übersetzungstests ausführen. Qt-Standardwidgets verwenden die installierten deutschen Qt-Übersetzungen.

Die Titelleiste zeigt `dfreernet <Version>` beziehungsweise `Dateiname — dfreernet <Version>`. Der Qt-Anzeigename bleibt kurz, damit die Plattform keinen zweiten ausgeschriebenen Produkttitel anhängt. Der Untertitel steht weiterhin in Dokumentation und Über-Dialog.

## Prüfung

GitHub Actions führt die referenzdatenfreie Suite `scripts/test.sh` für RELEASE und DEBUG in einem Fedora-44-Container aus. Qt läuft ohne Bildschirm; der Job greift nicht auf physische CAN-Hardware zu. Lokale Tests mit privaten Referenzdaten und virtuellem CAN bleiben getrennt.

```bash
scripts/test.sh
# Optionaler isolierter Heartbeat-Test; benötigt unshare/vcan-Unterstützung:
unshare --user --map-root-user --net python3 scripts/test_can_heartbeat.py
```

Integrationstests nicht auf eine echte CAN-Schnittstelle richten. Standardprüfungen arbeiten offline. Version 1.5.1 registriert 24 Offline-Testziele ohne zusätzliche referenzdatenabhängige Ziele. Weitere Fälle werden bei vorhandenen privaten Referenzdaten aktiviert. Sichtbare Änderungen durch Qt-Screenshots prüfen; die Tests speichern bereits Einstellungs- und Startansichten unter `analysis/`. Physischer Zugriff erfordert einen stillstehenden Rollstuhl und einen ausdrücklich autorisierten Vorgang. Aus einem Auftrag zur Quelltextänderung ergibt sich keine Schreibfreigabe.

## Lokale Artefakte und Wiederherstellung

`scripts/package.sh` erstellt `dist/dfreernet/bin` mit Anwendung und benötigten Controller-Hilfsprogrammen sowie zweisprachiger Dokumentation. Dieses Verzeichnis ist auf Linux-Rechner mit kompatiblem Qt 6 einschließlich QSQLITE-Treiber und zlib verschiebbar. Diese Systembibliotheken werden nicht mitgeliefert.

Buildverzeichnisse, Python-Caches, Mitschnitte und Sicherungsarchive sind generiert/privat und werden ignoriert. Quelltext, Referenzdaten und Dokumentation von Laufzeitmitschnitten trennen. Benutzerkonfigurationen beim Aufräumen nicht löschen.

Die rein lokalen Dateien `QUELLSTAND.sha256`, `LETZTE_SICHERUNG.txt` und `CHECKPOINT.json` dokumentieren Entwicklungsprüfsummen und wiederherstellbare Sicherungen. Sie bleiben zusammen mit Mitschnitten, persönlichen Konfigurationen und Build-Ausgaben außerhalb von Git. Frühere lokale Nachweise bleiben beim ursprünglichen Entwickler erhalten.

Eigenständig erstellter Code steht im Geltungsbereich von `SCOPE.de.md` unter GPL-3.0-only. Das lokale Programmverzeichnis enthält ausgenommene abgeleitete Katalog-, Text- und Vorlagendaten; Herkunft und Weitergabegrenzen stehen in `resources/PROVENANCE.de.md`. Die Codelizenz erlaubt keine Weitergabe der vollständig gebündelten Anwendung.

## Schlafsperre und Heartbeat-Wiederverbindung

`SleepInhibitor` verwendet asynchrone QtDBus-Aufrufe: einen logind-Dateideskriptor für `sleep:idle` (Fallback `sleep`) sowie ein Desktop-Inhibit-Cookie. Handles werden beim Deaktivieren/Beenden freigegeben; verspätete Antworten nach Abbruch oder Objektlöschung werden ebenfalls aufgeräumt. Die Sperre läuft nur im Hauptfenster, nicht in Controller-Hilfsprogrammen. Tests verwenden einen privaten D-Bus mit simulierten Diensten und prüfen Halten, Freigabe und verspätete Antworten. Standard-Oberflächentests deaktivieren echte Schlafsperren.

Der passive CAN-Indikator prüft zusätzlich Schnittstellenindex und IFF_UP. Nach USB-Neuanmeldung wird der Socket neu gebunden. Der virtuelle Regressionstest unterdrückt absichtlich die alte Fehlerbenachrichtigung und ersetzt die Schnittstelle; das Herz muss anschließend wieder auf echte Test-Heartbeat-Frames reagieren. Er prüft drei Testsendeframes vor und einen nach dem Schnittstellenwechsel, null vom Indikator.

Primärquellen: [systemd Inhibitor Locks](https://systemd.io/INHIBITOR_LOCKS/), [Idle Inhibition Service](https://specifications.freedesktop.org/idle-inhibit/0.1/index.html).

## Optionales Wine-Programmer-Unterprojekt

[`wine/README.de.md`](../wine/README.de.md) beschreibt die separate PE32-`ftd2xx.dll`,
die lokale Verarbeitung von USB-Kommandos und den nativen UDP-/SocketCAN-Proxy.
`wine/build.sh` baut das Unterprojekt; `python3 wine/tests/offline.py` prüft die
DLL unter Wine ohne CAN-Zugriff. `wine/run.sh --prepare-only` legt eine private
Kopie des installierten OEM-Programmers an. Der tatsächliche Start mit
`wine/run.sh` leitet CAN-Anforderungen des Programmers an `can0` weiter und
benötigt ausdrücklich freigegebene, stationäre Hardware. Die native Anwendung
ist nicht von dieser DLL abhängig.

## Linux-Begleitsteuerung

[`qt_attendant`](../qt_attendant/README.de.md) startet ohne Optionen mit geprüftem privatem Profil auf `can0`. `run.sh` richtet vor dem Live-Start `can0` und `can1` mit 125000 Bit/s ein (`RNET_BITRATE` ist optional). Bereits korrekt aktive Adapter bleiben unverändert; erforderliche Änderungen beachten die gemeinsame Schnittstellen-Sperre. Nur wegen fehlender Berechtigung gescheiterte `ip link set`-Befehle werden mit `sudo -n` wiederholt. Die Oberfläche läuft als normaler Benutzer. Einrichtungsfehler verhindern ihren Start; bereits ausgeführte Einrichtungsschritte werden nicht zurückgesetzt. Offline-/Empfangs-/Wiedergabe-/Hilfe- und Oberflächenprüfstarts überspringen diese Einrichtung. Direkte Binärstarts brauchen bereits eingerichtete Adapter.

Getrennte Protokollzustände prüfen Anmeldung, Gerätebestand, Konfiguration, Bereitschaft, Übernahme, Moduswechsel und Rückgabe. Joystick-Achsen reichen bis ±100; Speed wird separat geregelt. Oberflächeneingaben werden alle 50 ms aktualisiert und laufen nach 250 ms ab. Der 10-ms-CAN-Timer kann alte Eingaben nicht erneuern. Fokuswechsel erhalten Anmeldung, Übernahme, Modus und Speed, stellen aber sofort neutral. Die laufende QML-Oberfläche aktualisiert im Hintergrund ausschließlich Neutral; nach Rückkehr ist eine neue Betätigung nötig. Ohne frische Oberflächeneingaben greift weiterhin die Zeitüberwachung. Power-down sperrt die Ausgabe bis zum nächsten `00C`. Fensterschließen, SIGINT und SIGTERM fordern vor dem Ende die bestätigte Rückgabe an.

Die Tests verwenden simulierte Adapterbefehle, reine Zustandsautomaten, gespeicherte private Referenzen oder die Offline-Oberfläche. Fehlende private Mitschnitte führen zum Überspringen der betroffenen Fälle. Die jüngsten Start-/Beendigungs-/Fokusänderungen sind offline geprüft; ein neuer gemeinsamer Live-Test steht aus. Mit `-DDFREERNET_BUILD_ATTENDANT=OFF` entfallen BGM und Qt-Quick-Abhängigkeiten.

Der Tray verwendet Qt Widgets (`QApplication`, `QSystemTrayIcon`, `QMenu`) neben Qt Quick. Einstellungen liegen unter `tray/startHidden` und `tray/minimize` in den App-QSettings. Diagnose ignoriert gespeicherte Tray-Einstellungen; `--no-tray` erlaubt sichtbaren Wiederzugriff. Ausblenden und Steuerungspause sind getrennt. Die ausdrückliche Pause erhält den Modultransport, gibt die Kontrolle zurück und sperrt Eingaben. Eine verspätete Übernahmebestätigung während der Pause wird sofort zurückgegeben; Fortsetzen wartet auf die Rückgabe und übernimmt nicht automatisch. Das zusätzliche Offline-Ziel `attendant_system_tray` simuliert die Tray-Verfügbarkeit. QML-Prüfungen bei ausgeblendetem Fenster prüfen neutrale Hintergrundeingaben; Menütests prüfen Einstellungen, Rückfallebene und Beenden.

`RegisteredResume` behandelt einen neuen App-Prozess am bereits laufenden Bus, wenn eine private `resume_reference` vorhanden ist. Wiederholter passender Fahrer-Heartbeat, C8/C9-Kennungen aller vier Gegenstellen in den erwarteten Slots, der bekannte siebenstufige Slot-0-POP-Lesedialog und der Vergleich beider variablen Werte samt ODI 0x40/0x50 sind Voraussetzung. Nur passende Antworten geben den bisherigen manuellen Übernahmeweg frei. Der Pfad sendet keine Geräteerkennung, Tabellenübertragung, Bereitschaftsresets, Profilwahl oder Joystickwerte. Der eigene Lesezugriff wird begrenzt aufgeräumt; ausdrückliche Ablehnungen fremder Sitzungen bleiben unangetastet. Das neue Offline-Ziel `attendant_registered_resume` prüft Annahme, falsche Kennungen/Clientadressen, veränderte Konfiguration, Konflikte, Fristen und Abbruch. `attendant_capture_decoder` prüft zusätzlich die Referenzextraktion. `tools/resume_reference.py` gewinnt die optionalen privaten Felder aus einem vollständigen Originalstart und erhält eine Sicherung. Der Benutzer hat Wiederaufnahme, Anmeldung/Deregistrierung und Steuerung am lokalen System bestätigt; breitere Hardware- und Fehlerfallprüfungen bleiben davon getrennt.

## Nativer IR-Editor

[`qt_iredit`](../qt_iredit/README.de.md) bearbeitet lokale RnIR3-Menüs und Pronto-Hex ohne Wine oder CAN. Der begrenzte native NRBF-Leser erhält Menüstruktur und beide Signalplätze; die physische Signalumrechnung bleibt experimentell. Der Hauptbuild enthält Offline-Prüfungen für Dateiformat und Qt-Widgets-Oberfläche.
