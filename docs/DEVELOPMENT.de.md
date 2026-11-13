# Entwicklung

[English](DEVELOPMENT.md) | [Deutsch](DEVELOPMENT.de.md)

## Bauen und starten

Die unterstützte Entwicklungsoberfläche ist eine native Linux-Anwendung mit C++17, CMake ab 3.21, Ninja, Python 3, zlib-Entwicklungsdateien und Qt ab 6.10 (Widgets, Sql, DBus, LinguistTools sowie Test für Tests). Im Betrieb werden keine zusätzlichen Netzwerkdienste benötigt. Auf dem aktuellen Entwicklungsrechner ist Qt 6.11.2 installiert.

```bash
scripts/build.sh
scripts/test.sh
scripts/run.sh --language de_DE
```

CMake-Ziel und Programmdatei: `dfreernet`, `build-native/dfreernet`. Buildkonfiguration: `RelWithDebInfo`; die Skripte verwenden zwei parallele Buildaufträge. Compilerwarnungen sind für native Quelltextziele aktiviert. Bei manuellem CMake-Aufruf dieselben Qt-Module und ein separates Buildverzeichnis verwenden.

Native Laufzeitressourcen werden aus geprüften Projektdateien unter `resources/` eingebettet. CMake startet `scripts/embed_resources.py`, das deren feste SHA256-Werte prüft und eine C++-Datei erzeugt. Das Hauptprogramm verlangt die ausdrückliche Zustimmung zur Startwarnung, bevor es die Dokument-/Controlleroberfläche erzeugt. `--quit-after` darf die Warnung für Oberflächenprüfungen ablehnen, aber nicht bestätigen.

## Quelltextübersicht

| Ort | Aufgabe |
| --- | --- |
| `src/qt/` | Widgets, Übersetzungen, Einstellungen, typisierte Editoren, Icons, Startwarnung, passiver Heartbeat |
| `src/document.*`, `src/database.*`, `src/values.*` | Dateierhaltung, Kataloglayout, typisierte Wertezuordnung |
| `src/can_reader.*`, `src/can_writer.*`, `src/can_socket.*` | Begrenzter Controllertransport und Lese-/Schreibzustandsautomaten |
| `src/program_controller.cpp` | Controller-Schreibhilfe der Qt-Anwendung |
| `tests/` | Offline-Tests, SocketCAN-Testprogramme und private Referenzdaten |
| `scripts/` | Aktuelle Build-/Start-/Prüfskripte sowie erhaltene Untersuchungswerkzeuge |
| `translations/rnet_de_DE.ts` | Deutscher Qt-Linguist-Katalog; englische Quellsprache |
| `docs/history/` | Historische chronologische Notizen; können überholte Implementierungen beschreiben |

Interne `rnet_*`-Bibliotheks-/Hilfsprogrammnamen, der Qt-Übersetzungskontext `RNet`, die Endung `.R-net` und historische Nachweisdateinamen bleiben aus Kompatibilitätsgründen erhalten. Sie sind nicht der angezeigte Produktname.

## SQLite-Katalog

`resources/dfreernet.sqlite3` ist die geprüfte, versionierte Datenquelle. Sie enthält strukturierte Tabellen für Parameter, Parametertexte/-felder, Speicherzuordnungen/-gruppen/-abschnitte/-elemente, Editoren und Optionen, Dateinamen, Menüs und Ressourcentexte. Opake Vorlagen-/Referenzdaten bleiben als ausdrücklich benannte BLOBs erhalten. 64-Bit-Speicherschlüssel werden verlustfrei als Dezimaltext gespeichert.

Beim Build werden Prüfsumme und SQLite-Integrität geprüft und die Datenbank komprimiert eingebettet. Zur Laufzeit wird eine private temporäre Kopie ausschließlich lesend über QtSql/QSQLITE geöffnet. Der Katalog wird direkt aus den Tabellen geladen. Das lokale Programmverzeichnis enthält außerdem eine prüfbare Datenbankkopie unter `share/dfreernet/`. Änderungen an dieser Kopie ersetzen den eingebetteten Laufzeitkatalog nicht.

RND-Importe erzeugen neue Datenbankdateien in einer Transaktion mit Rückprüfung; vorhandene Dateien werden abgewiesen. Der importierte Katalog wird nicht automatisch für Controllerzugriffe aktiviert. Entwicklungswerkzeuge für Tabellenexport und Datenbankerzeugung liegen unter `scripts/export_catalog_tables.cpp` und `scripts/build_catalog_sqlite.py`; normale Builds importieren keine RND-Datei.

## Einstellungen und Übersetzung

Anwendungsidentität: `LocalRNet` / `dfreernet`, Version 1.2.0. Qt QSettings speichert Einstellungen am plattformüblichen Ort (unter Linux normalerweise `$XDG_CONFIG_HOME/LocalRNet/dfreernet.conf`). Beim ersten Start unter dem neuen Namen werden Schlüssel aus `LocalRNet` / `R-net Qt` nur übernommen, wenn der neue Speicher leer ist. Der alte Speicher bleibt erhalten. Andere gespeicherte Pfade bleiben bestehen; neue Standardpfade für Anwendungsdaten verwenden den neuen Namen.

US-Englisch ist die Quellsprache; Deutsch verwendet eine kompilierte Qt-`.qm`-Ressource. Übersetzungstexte im vorhandenen Kontext `RNet` ergänzen, Platzhalter erhalten, den `.ts`-Katalog aktualisieren und Übersetzungstests ausführen. Qt-Standardwidgets verwenden die installierten deutschen Qt-Übersetzungen.

Die Titelleiste zeigt `dfreernet <Version>` beziehungsweise `Dateiname — dfreernet <Version>`. Der Qt-Anzeigename bleibt kurz, damit die Plattform keinen zweiten ausgeschriebenen Produkttitel anhängt. Der Untertitel steht weiterhin in Dokumentation und Über-Dialog.

## Prüfung

```bash
scripts/test.sh
# Optionaler isolierter Heartbeat-Test; benötigt unshare/vcan-Unterstützung:
unshare --user --map-root-user --net python3 scripts/test_can_heartbeat.py
```

Integrationstests nicht auf eine echte CAN-Schnittstelle richten. Standardprüfungen arbeiten offline. Mit lokalen privaten Referenzdaten umfasst die Suite 17 Testziele; der private Git-Quelltextstand führt die referenzdatenfreien Tests aus. Sichtbare Änderungen durch Qt-Screenshots prüfen; die Tests speichern bereits Einstellungs- und Startansichten unter `analysis/`. Physischer Zugriff erfordert einen stillstehenden Rollstuhl und einen ausdrücklich autorisierten Vorgang. Aus einem Auftrag zur Quelltextänderung ergibt sich keine Schreibfreigabe.

## Lokale Artefakte und Wiederherstellung

`scripts/package.sh` erstellt `dist/dfreernet/bin` mit Anwendung und benötigten Controller-Hilfsprogrammen sowie zweisprachiger Dokumentation. Dieses Verzeichnis ist auf Linux-Rechner mit kompatiblem Qt 6 einschließlich QSQLITE-Treiber und zlib verschiebbar. Diese Systembibliotheken werden nicht mitgeliefert.

Buildverzeichnisse, Python-Caches, Mitschnitte und Sicherungsarchive sind generiert/privat und werden ignoriert. Quelltext, Referenzdaten und Dokumentation von Laufzeitmitschnitten trennen. Benutzerkonfigurationen beim Aufräumen nicht löschen.

Die rein lokalen Dateien `QUELLSTAND.sha256`, `LETZTE_SICHERUNG.txt` und `CHECKPOINT.json` dokumentieren Entwicklungsprüfsummen und wiederherstellbare Sicherungen. Sie bleiben zusammen mit Mitschnitten, persönlichen Konfigurationen und Build-Ausgaben außerhalb von Git. Frühere lokale Nachweise bleiben beim ursprünglichen Entwickler erhalten.

Eigenständig erstellter Code steht im Geltungsbereich von `SCOPE.de.md` unter GPL-3.0-only. Das lokale Programmverzeichnis enthält ausgenommene abgeleitete Katalog-, Text- und Vorlagendaten; Herkunft und Weitergabegrenzen stehen in `resources/PROVENANCE.de.md`. Die Codelizenz erlaubt keine Weitergabe der vollständig gebündelten Anwendung.

## Schlafsperre und Heartbeat-Wiederverbindung

`SleepInhibitor` verwendet asynchrone QtDBus-Aufrufe: einen logind-Dateideskriptor für `sleep:idle` (Fallback `sleep`) sowie ein Desktop-Inhibit-Cookie. Handles werden beim Deaktivieren/Beenden freigegeben; verspätete Antworten nach Abbruch oder Objektlöschung werden ebenfalls aufgeräumt. Die Sperre läuft nur im Hauptfenster, nicht in Controller-Hilfsprogrammen. Tests verwenden einen privaten D-Bus mit simulierten Diensten und prüfen Halten, Freigabe und verspätete Antworten. Standard-Oberflächentests deaktivieren echte Schlafsperren.

Der passive CAN-Indikator prüft zusätzlich Schnittstellenindex und IFF_UP. Nach USB-Neuanmeldung wird der Socket neu gebunden. Der virtuelle Regressionstest unterdrückt absichtlich die alte Fehlerbenachrichtigung und ersetzt die Schnittstelle; das Herz muss anschließend wieder auf echte Test-Heartbeat-Frames reagieren. Er prüft drei Testsendeframes vor und einen nach dem Schnittstellenwechsel, null vom Indikator.

Primärquellen: [systemd Inhibitor Locks](https://systemd.io/INHIBITOR_LOCKS/), [Idle Inhibition Service](https://specifications.freedesktop.org/idle-inhibit/0.1/index.html).
