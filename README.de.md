# dfreernet

[English](README.md) | [Deutsch](README.de.md)

**Dongle free R-Net Programmer** · Native Linux-Anwendung · C++17 / Qt 6 / SocketCAN

Experimentelle Desktopanwendung zum Lesen von R-Net-Controllerkonfigurationen, Bearbeiten unterstützter Parameter und Speichern von Konfigurationsdateien. Die Verbindung erfolgt über Linux SocketCAN ohne Programmierdongle, Wine oder FTDI-/CUSBMsg-Laufzeit-DLLs. Der geprüfte Parameterkatalog, Menü-/Textressourcen, kryptografische Konstanten und die Konfigurationsvorlage sind in die nativen Binärdateien eingebettet. Build und Betrieb verwenden ausschließlich eingecheckte Ressourcen und installierte Linux-Bibliotheken.

**Lizenz:** Eigenständig erstellter Code steht unter [GPL-3.0-only](LICENSE). Der eingebettete Katalog und weitere von Dritten abgeleitete Inhalte sind ausgenommen; siehe [Geltungsbereich](SCOPE.de.md). Die Weitergabe der vollständigen Anwendung ist damit nicht freigegeben.

**Teststand: rudimentär auf Permobil T5 VS getestet.** Dies sind begrenzte Entwicklungsversuche, keine allgemeine Kompatibilitätszusage oder Herstellerfreigabe. Andere Rollstuhlmodelle, Steuerungsvarianten und Firmwarestände sind nicht bestätigt. Siehe [Teststand und Grenzen](docs/VALIDATION.de.md).

## Version 1.2.2

Diese Version verwendet eine gemeinsame CAN-Zähltabelle für Mitschnitt und passive Systemtests. Beide Ansichten bieten Symbolschalter für Verlauf/Zählung und Hz/ms sowie einstellbare Spalten und gespeicherte Layout- und Ansichtsoptionen. Sie bleibt experimentell: Controllerkompatibilität und Schreibpfad sind nur teilweise verifiziert. Private Controller-Referenzdaten, Mitschnitte und Sicherungen fehlen im Repository; lokale Referenztests benötigen sie.

## Bildschirmfotos (mit 1.29999.2 aufgenommen)

Hauptfenster mit Parametertabelle und Berechtigungsstufen:

![dfreernet-Hauptfenster mit Parametertabelle](docs/screens/Bildschirmfoto%20vom%202026-11-14%2006-28-20.png)

Startwarnung mit illustriertem Rollstuhl:

![dfreernet-Startwarnung](docs/screens/Bildschirmfoto%20vom%202026-11-12%2020-12-57.png)

<details>
<summary>Weitere Bildschirmfotos: Dongle-Hinweis und Über-Dialog</summary>

![Warnung zur Dongle-Berechtigung](docs/screens/Bildschirmfoto%20vom%202026-11-12%2009-28-23.png)

![Über dfreernet](docs/screens/Bildschirmfoto%20vom%202026-11-12%2011-16-00.png)

</details>

## Schnellstart

Benötigt werden Linux, ein C++17-Compiler, CMake ab 3.21, Ninja, Python 3, zlib-Entwicklungsdateien sowie Qt ab 6.10 mit Widgets/Sql/DBus/LinguistTools und Qt Test für den Standard-Testbuild. Auf dem Entwicklungsrechner wird Qt 6.11.2 verwendet. Controllerzugriffe benötigen einen eingerichteten SocketCAN-Adapter.

```bash
scripts/build.sh
scripts/build.sh DEBUG  # getrennter Debug-Build
scripts/run.sh
# Gespeicherte Konfiguration öffnen:
scripts/run.sh --document '/absoluter/pfad/konfiguration.R-net'
# Offline-Tests ausführen:
scripts/test.sh
```

Die Programmdateien liegen unter `build/RELEASE/dfreernet` (Standard) und `build/DEBUG/dfreernet`. Mit `DFREERNET_BUILD_MODE=DEBUG` nutzen `scripts/run.sh`, `scripts/test.sh`, `scripts/package.sh` und Python-Integrationstests den Debug-Build. Das Startskript funktioniert unabhängig vom Ort dieses Projekts. `scripts/package.sh` erstellt ein verschiebbares lokales Programmverzeichnis; Start mit `dist/dfreernet/bin/dfreernet`. Das vollständige Verzeichnis einschließlich Controller-Hilfsprogrammen kopieren. Auf dem Zielrechner müssen Qt ab 6.10 einschließlich Sql-Modul/QSQLITE-Treiber und zlib installiert sein; Wine und Originalprogramm werden nicht benötigt.

Bei jedem interaktiven Start erscheint eine Warnung mit Haftungsausschluss. Erst die bewusste Auswahl von „Fortfahren“ öffnet die Anwendung. Standardsprache ist US-Englisch; Deutsch ist unter Sprache beziehungsweise **Bearbeiten → Einstellungen → Allgemein** verfügbar. Vorhandene Einstellungen der früheren Anwendung „R-net Qt“ werden einmalig übernommen, sofern noch keine dfreernet-Einstellungen existieren.

## Funktionen

- Controllerkonfiguration lesen und empfangene Nutzdaten einschließlich unbekannter Blöcke erhalten.
- Zum installierten Katalog und zur gelesenen Dateiversion passende Parameter anzeigen und nach Berechtigung filtern.
- Unter Ansichten zwischen Kategorienliste, Profilansicht und Modulgruppen wechseln. Die Profilansicht zeigt links Allgemein und Profil 1–8; die Modulansicht gruppiert nach Katalogmodulen. Alle Ansichten nutzen dieselben gelesenen Werte, Suche, Berechtigungen und Editoren.
- Die drei Ansichtsschaltflächen stehen direkt neben dem Suchfeld.
- Spaltenbreiten und -reihenfolge bleiben für jede Tabelle automatisch erhalten; unter Ansichten gibt es zusätzlich Speichern, Laden und Zurücksetzen.
- Die Profilansicht zeigt das zugehörige Modul und die Gruppe; gleich benannte Parameter mit unterschiedlichen IDs bleiben unterscheidbar.
- Eine kompakte Spalte direkt nach dem Rohwert zeigt Katalogeinheit, Skalierungsfelder und Rohwert-Minimum/-Maximum. Die farbige Berechtigung bleibt rechts auch beim horizontalen Scrollen sichtbar. Eine ungeprüfte Umrechnung in technische Einheiten erfolgt nicht.
- Unterstützte Rohwerte mit geprüften Ja/Nein-, Auswahl-, Zahlen- und Texteditoren bearbeiten; Änderungen erscheinen rot und fett.
- F1 auf einem ausgewählten Parameter öffnet den eingebetteten R-net-Hilfetext mit ID, Profil, Rohwert und Berechtigung.
- `.R-net`-Dateien öffnen und speichern; angezeigte Informationen exportieren.
- Datei → RND-Katalog importieren… prüft eine RND-Datei und schreibt deren Tabellen in eine neue SQLite-Datenbank. Der eingebettete Katalog wird nicht automatisch gewechselt.
- Unterstützte Änderungen mit Sicherung, Vergleich und Rückleseprüfung übertragen. Der Schreibpfad ist begrenzt und experimentell.
- Modulkennungen und Seriennummern abfragen. Unter der fixierten Berechtigungsspalte läuft ein fortlaufender CAN-Mitschnitt nur mit Empfang; „CAN beobachten“ erstellt weiterhin eine zeitlich begrenzte Zusammenfassung.
- Den waagerechten Trenngriff für die Höhe und den Trenngriff neben dem CAN-Mitschnitt für dessen Breite ziehen; die Positionen bleiben nach dem Neustart erhalten.
- Passiven CAN-Heartbeat als pulsierendes Herz rechts in der Statusleiste anzeigen.
- Linux während der App-Nutzung wach halten; optionales Warnblinker-Signal nur für die aktuelle Sitzung (Standardintervall 300 s, Dauer 60 s; verlangt die Bestätigung „sichtbar aus“ und verhindert den Rollstuhl-Ruhezustand nicht).
- CAN-, Ordner-, Sprach- und Darstellungseinstellungen in vertikalen Reitern speichern.

## Dokumentation

| Dokument | Inhalt |
| --- | --- |
| [Bedienung](docs/USER-GUIDE.de.md) | Einrichtung und Arbeitsabläufe |
| [Teststand und Grenzen](docs/VALIDATION.de.md) | Tatsächliche Erprobung, Nachweise und offene Punkte |
| [Entwicklung](docs/DEVELOPMENT.de.md) | Build, Struktur, Übersetzungen und Prüfungen |
| [Änderungen](CHANGELOG.de.md) | Aktueller Änderungsstand |

## Projektstruktur

```text
src/             Nativer Kern, SocketCAN-Werkzeuge und Qt-Oberfläche
translations/    Deutscher Qt-Linguist-Katalog; englische Quelltexte
scripts/         Build, Start, Paketierung, virtuelle CAN-Tests und Katalogwerkzeuge
tests/           Automatisierte Tests; private Referenzdaten bleiben lokal
resources/       Geprüfter SQLite-Katalog und Datenherkunft
docs/            Aktuelle zweisprachige Dokumentation
analysis/        Private Mitschnitte, Berichte und Screenshots (generiert)
checkpoints/     Wiederherstellbare lokale Sicherungen
build/           Generierte DEBUG- und RELEASE-Builds
```

Historische Untersuchungen, lokale Sicherungen, Controller-Mitschnitte, persönliche Konfigurationen und Build-Ausgaben bleiben außerhalb von Git. Das Repository enthält den Quellcode der nativen Anwendung und den festgelegten Laufzeitkatalog; die eingebetteten abgeleiteten Daten sind von der Codelizenz ausgenommen.

## Nutzung und Weitergabe

Unabhängige experimentelle Software; kein offizielles Werkzeug von Permobil oder dem Steuerungshersteller. Falsche Einstellungen können den Rollstuhl blockieren oder unbeabsichtigte Bewegungen auslösen. Vor Controllerzugriffen die Programmwarnung und [Bedienungsanleitung](docs/USER-GUIDE.de.md) lesen. Eine unabhängige Sicherung behalten und Änderungen durch Rücklesen prüfen.

Der eigenständig erstellte Code steht im beschriebenen [Geltungsbereich](SCOPE.de.md) unter GPL-3.0-only. Die eingebetteten Daten wurden einmalig aus lokal geprüften Referenzressourcen abgeleitet; siehe [Datenherkunft](resources/PROVENANCE.de.md). Laufzeitunabhängigkeit begründet keine Weitergaberechte an diesen Daten. Dieses Projekt erteilt keine Weitergaberechte an Originalsoftware, Katalogen, Dokumenten, Mitschnitten oder Binärdateien. Produkt- und Firmennamen dienen der Beschreibung des Kompatibilitätskontexts; eine Unterstützung durch deren Inhaber wird nicht behauptet.
