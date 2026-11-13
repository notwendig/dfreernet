# dfreernet — Bedienung

[English](USER-GUIDE.md) | [Deutsch](USER-GUIDE.de.md)

**Dongle free R-Net Programmer**, experimentelle Qt-6-Anwendung für Linux.

**Rudimentär auf Permobil T5 VS getestet.** Die vorhandenen Versuche belegen keine allgemeine Kompatibilität und ersetzen keine vollständige Erprobung. Andere Modelle und Firmwarestände sind nicht bestätigt.

## Voraussetzungen und Start

Linux mit Qt 6.10 oder neuer und einer passenden SocketCAN-Schnittstelle. Die Anwendung richtet den CAN-Adapter nicht selbst ein. Schnittstelle, Verkabelung und Bitrate müssen zur tatsächlichen Steuerung passen. Auf dem bisherigen Entwicklungsaufbau wurde `can0` mit 125 kbit/s verwendet; das ist keine automatische Vorgabe für andere Systeme.

Der geprüfte Katalog, Oberflächentexte und die Snapshotvorlage sind eingebettet. Die App verwendet ihre eingebetteten Ressourcen und Linux SocketCAN. Ein kopiertes eigenständiges Programmverzeichnis über `bin/dfreernet` starten und die Controller-Hilfsprogramme daneben erhalten. Installiertes Qt ab 6.10 einschließlich Sql-Modul/QSQLITE-Treiber und zlib bleiben erforderlich.

```bash
scripts/run.sh
```

Das modale Warnfenster erscheint bei jedem Start. „Verstanden – Fortfahren“ öffnet die Oberfläche, „Beenden“ und Escape schließen den Start. Beim ersten Start öffnet sich das Fenster nach Möglichkeit auf dem zweiten Monitor. Danach werden Position, Größe und Maximierungszustand wiederhergestellt; die Warnung erscheint auf demselben Bildschirm. Im Menü „Sprache“ kann zwischen US-Englisch und Deutsch gewechselt werden.

## Bearbeiten → Einstellungen

Vier vertikale Reiter bündeln die dauerhaft gespeicherten Einstellungen:

| Reiter | Inhalt |
| --- | --- |
| Allgemein | Sprache, Standard-Dongle-Berechtigung, Rückgängig-Schritte |
| CAN | Schnittstelle, Monitorzeit, Herz-Anzeige und Empfangszeitlimit |
| Dateien und Ordner | Arbeits-, Sicherungs-, Export- und Protokollordner |
| Darstellung | Detailbereich, Icon- und Schriftgröße |

„Anwenden“ übernimmt gültige Werte, „OK“ übernimmt und schließt, „Abbrechen“ verwirft noch nicht übernommene Eingaben. Der Protokollordner wirkt beim nächsten Start. Explizite Startparameter haben Vorrang. Die gespeicherte CAN-Schnittstelle und die Sicherungsordner werden bei Controlleraktionen ohne erneute Auswahl verwendet. Sicherheitsbestätigungen bleiben erhalten; beim Öffnen oder Speichern einer Datei wird weiterhin ein Dateiname ausgewählt.

## Konfiguration lesen

Den Rollstuhl abstellen; während des Zugriffs darf der Fahrbetrieb nicht benötigt werden. „Controller lesen“ wählen und den stationären Zustand bestätigen. Auch Lesen kann einen Programmierzugang belegen und den Fahrbetrieb vorübergehend blockieren.

Empfangene Dateien werden in einem neuen eindeutigen Unterordner des Sicherungsordners gespeichert. Die Oberfläche öffnet bevorzugt die Konfiguration des verifizierten Repository-Knotens 2. Unterstützte Parameter erscheinen als Werte; unbekannte Dateiversionen bleiben als Rohdaten erhalten und werden nicht als bekannte Parameter ausgegeben.

## Parameter bearbeiten und Dateien speichern

Unter **Ansichten** zwischen **Hierarchische Liste**, **Profiltabelle** und **Modulgruppen** wechseln. Die Profilansicht bietet links Allgemein und Profil 1–8. Die Modulansicht gruppiert nach den ersten beiden Teilen des Katalogpfads. Rechts erscheinen die Parameter der Auswahl. Ein Profil zeigt nur Parameter, die im Katalog dem Profilspeicher zugeordnet sind; passende ungelesene Parameter sind grau, wenn sie in den Einstellungen aktiviert sind. Suche, Berechtigungsfilter und CSV-Export funktionieren in allen drei Ansichten. Die Auswahl wird gespeichert.

Spaltenbreiten und -reihenfolge werden nach Änderungen automatisch gespeichert und beim erneuten Öffnen einer Ansicht oder nach einem Neustart wiederhergestellt. **Ansichten → Spalteneinstellungen speichern** sichert sofort; **Spalteneinstellungen laden** stellt den zuletzt gespeicherten Stand wieder her und **Spalteneinstellungen zurücksetzen** die Standardwerte. Jede Tabellenansicht hat ein eigenes Layout.

Die Spalte „Modul / Gruppe“ unterscheidet gleich benannte Parameter verschiedener R-net-Module. Ein Zusatz wie „/ 2“ bezeichnet das zweite Element beziehungsweise den zweiten Kanal.

Die Spalte direkt nach dem Rohwert fasst Einheit, Skala und Rohwert-Minimum/-Maximum zusammen (`Einheit · a:b · min–max`). Der Spaltentitel ist zweizeilig; die Werte bleiben einzeilig. Der Tooltip benennt jedes Feld. Die Einheit stammt aus Symbol und Vorsatz des Katalogs; die numerischen Grenzen sind rohe Katalogwerte. Diese Metadaten rechnen den gespeicherten Rohwert nicht in technische Einheiten um. Die farbige Berechtigung bleibt beim horizontalen Scrollen rechts fest sichtbar.

Suche und Gruppenbaum begrenzen die Anzeige. Doppelklick oder F2 in der Rohwertspalte oder die Schaltfläche **Wert bearbeiten…** für einen ausgewählten editierbaren Parameter öffnet denselben Editor in der Tabelle: Ja/Nein, Auswahl, Ganzzahl oder Text. Enter übernimmt, Escape verwirft die Eingabe. Grenzen, Schrittweite und Textkodierung werden geprüft. Ungeklärte Sondertypen bleiben schreibgeschützt.

Einen Parameter auswählen und **F1** drücken (oder **Hilfe → Parameterhilfe**), um seinen eingebetteten Kataloghilfetext mit ID, Gruppe, Profil, Rohwert und Berechtigung zu sehen. Die Anleitung „R-net PC Programmer“ beschreibt diese Parameterhilfe in Abschnitt 3.3.2. Das Hilfefenster funktioniert ohne Originalinstallation.

Änderungen erscheinen rot und fett. Rückgängig stellt vorherige Werte wieder her. „Datei speichern“ beziehungsweise „Datei schreiben unter“ speichert eine Arbeitskopie; dies überträgt keine Werte auf den Controller und ist keine Bestätigung eines Controller-Schreibvorgangs.

Die Spalte Berechtigung zeigt 0 Therapeut (gelb), 1 Händler (grün), 2 OEM (blau), 3 Werk (rot). Das Dongle-Menü begrenzt Anzeige und freigegebene Bearbeitung. Diese lokale Auswahl ersetzt keine Herstellerqualifikation und garantiert keine vom Controller erteilte Berechtigung.

## Auf Controller schreiben

Der aktuelle Pfad unterstützt nur verifizierte direkte Parameterzuordnungen und den geprüften Zielknoten. Er ist kein universeller Firmware-Flasher. Die Vorschau prüfen, den Rollstuhl stillstehen lassen und eine Sicherung behalten. Der Schreibablauf vergleicht den aktuellen Controllerstand, sichert Daten und fordert anschließend Rücklesen an.

Ein bestätigter Commit ohne verifiziertes Rücklesen ist **kein vollständig bestätigter Erfolg**. Änderungen können bereits aktiv sein. In diesem Fall nicht erneut schreiben: Controller lesen, tatsächliche Werte und Protokoll prüfen. Mehrere Dateien sind keine atomare Transaktion; ein Fehler kann einen teilweise geänderten Stand hinterlassen.

## RND-Katalog importieren

Unter **Datei → RND-Katalog importieren…** eine RND-Datei auswählen. RND enthält Parameterdefinitionen, Hilfetexte, Grenzen, Berechtigungen, Auswahllisten und Speicherzuordnungen. Konkrete Rollstuhleinstellungen stehen in `.R-net`-Dateien.

Der Import entschlüsselt und prüft das unterstützte Katalogformat und fragt nach dem Namen einer neuen `.sqlite3`-Datei. Bestehende Dateien werden nicht überschrieben. Parameter, Textfelder, Metadaten, Speicherzuordnungen und Editoren werden in strukturierten SQLite-Tabellen gespeichert; die Datenbank wird danach geprüft. Der eingebaute Katalog und Controllerdaten bleiben unverändert. Ein ungültiges oder nicht unterstütztes Format führt zum Abbruch.

## Linux während der App-Nutzung wach halten

Unter **Bearbeiten → Einstellungen → Allgemein** lässt sich „Linux-Schlafmodus verhindern, solange die App geöffnet ist“ ein- oder ausschalten. Die Option ist standardmäßig aktiv. Die App fordert eine temporäre Systemsperre gegen automatischen Suspend und, soweit vom Desktop unterstützt, eine Leerlauf-/Bildschirm-Sperre an. „Linux wach“ rechts in der Statusleiste zeigt eine erhaltene Systemsperre; der Tooltip nennt Teilfunktionen oder fehlende Unterstützung. Ausschalten der Option oder Schließen der App gibt die Sperren frei. Systemeinstellungen werden nicht dauerhaft geändert.

Das betrifft den Linux-Rechner. Der Sleep Timer des Rollstuhls bleibt unverändert. Ein passiver 30-Minuten-Test zeigte, dass Lichtbefehle den getesteten T5 VS nicht wach halten. Der Lichtzyklus startet niemals automatisch.

## CAN-Herz und Identifikation

Rechts in der Statusleiste pulsiert das Herz rot, wenn aktuelle vollständige Geräte-Heartbeat-Frames `0x03C30F0F` mit sieben Bytes `0x87` empfangen werden. `0x00E` ist hier eine Serienkennung und aktiviert das Herz nicht. Ohne aktuellen Empfang ist es grau und still. Eine ausgeschaltete Schnittstelle steht ausdrücklich im Tooltip. Nach USB-/Schnittstellenwechsel bindet sich der Empfänger automatisch neu; er konfiguriert den Adapter nicht und sendet keine CAN-Nachrichten. Die Anzeige hört ausschließlich mit und sendet keine CAN-Nachrichten. Sie bestätigt weder Fahrbereitschaft noch Schreibberechtigung. Ein graues Herz kann auch durch eine falsche Schnittstelle oder einen auf diesem System abweichenden Heartbeat entstehen.

Seriennummernabfrage und CAN-Monitor verwenden die gespeicherte Schnittstelle. Eine Seriennummer ist nicht automatisch ein zuverlässig ermittelter Modulname.

Unter der fixierten Berechtigungsspalte empfängt ein Fenster fortlaufend CAN-Nachrichten. **Verlauf** zeigt die letzten 2000 Frames mit Empfangszeit, Schnittstelle, Kennung, DLC und Nutzdaten. **Zählung** gruppiert nach Kennung und Frame-Typ in einer Tabelle: Spalte 0 ist ein achtstelliger Zähler, gefolgt von einer rechtsbündigen CAN-ID und den letzten Nutzdaten. Beim Zurückschalten erscheint wieder der jüngste Verlauf. Bei einem Wechsel der gewählten Schnittstelle beginnen die Zähler neu. Der Empfänger verbindet sich nach Wiederkehr der Schnittstelle erneut und sendet nichts. **Controller → CAN beobachten…** bleibt eine getrennte, zeitlich begrenzte Zusammenfassung.

**Werkzeuge → Systemtests** öffnet passive Live-Ansichten für alle zehn Menüpunkte. Jede Tabelle hat denselben achtstelligen Zähler in Spalte 0 und eine rechtsbündige CAN-ID in Spalte 1. PM, JSM, Joystickstatus und Zwischenwerte XY filtern dokumentierte CAN-Frame-Familien; die Joystickansichten zeigen rohe X/Y-Bytes als vorzeichenbehaftete Werte. Die übrigen Modulansichten zeigen nicht zugeordneten rohen Busverkehr, da dessen Statuskodierung nicht geprüft ist. Beim Öffnen oder Wechseln eines Tests beginnen die Zähler neu. Diese Ansichten bilden die vollständigen Diagnosefelder des Originalprogramms nicht nach und senden keine Anfragen. Das [R-net-Programmer-Handbuch](https://medema.no/files/Programmer%20k%C3%B8restole/R-net_PC_Programmer_OEM.pdf) beschreibt die Originaltests.

Der waagerechte Trenngriff über den unteren Fenstern verändert deren Höhe. Der senkrechte Trenngriff links vom CAN-Mitschnitt verändert dessen Breite. Die Spalten in den CAN-Tabellen lassen sich an den Kopfzeilen ziehen. Bei geöffnetem Vorgangsprotokoll verstellt ein weiterer Trenngriff links Details und Protokoll getrennt. Die Trennerpositionen werden beim Schließen gespeichert.

## Bei Problemen

Schnittstelle und Ordner zuerst in Einstellungen prüfen. Sicherung, Protokoll und verwendete Programmversion aufbewahren. Nach einer unbestätigten Übertragung keinen Schreibversuch wiederholen, bevor der tatsächliche Stand gelesen ist. Ein vollständiger Wiederherstellungsablauf ist nicht für alle Steuerungsvarianten nachgewiesen.

## Optionales Live-Signal: Warnblinker

Unter **Bearbeiten → Einstellungen** bietet die CAN-Seite den Lichtzyklus-Schalter, das Intervall (Standard **300 Sekunden**) und die Impulsdauer (Standard **60 Sekunden**). Auch das CAN-Menü startet oder beendet den Zyklus. Er ist **bei jedem App-Start aus**; ein gespeicherter Ein-Wert wird ignoriert.

Zum Start ist ein aktueller Heartbeat nötig. Vorher fragt die App, ob der Rollstuhl abgestellt und der Warnblinker **sichtbar AUS** ist. Der Befehl schaltet nur um; ein gemessener Lampenzustand fehlt. Ist das Licht bereits an, bleibt es zwischen den Impulsen an. Das Statussymbol zeigt den Zeitablauf, nicht den tatsächlichen Lampenzustand.

Der erste Umschaltbefehl erfolgt sofort, der zweite nach der eingestellten Dauer. Fehlt der CAN-Empfang oder ist der Controllerzugang blockiert, stoppt der Zyklus und startet nicht selbst wieder. Ausschalten oder Schließen versucht ein laufendes Paar abzuschließen. Bei Sendefehlern oder einer anderen Betätigung die Lampen sichtbar prüfen.

Ein passiver 31-Minuten-Test vom 11. November 2026 erfasste 31 Umschaltpaare. Der Rollstuhl schaltete exakt 30 Minuten nach der letzten Joystickaktivität ab, nur 37 Sekunden nach dem letzten Paar. Der Zyklus **verhindert das automatische Abschalten** am getesteten T5 VS **nicht**. Controllerparameter bleiben unverändert. Ein CLI-Start mit `scripts/run.sh --hazard-blink-test` braucht ebenfalls die sichtbare Bestätigung und einen aktuellen Heartbeat.

Unter **Darstellung → Werkzeugleiste: nur Symbole** lassen sich die Beschriftungen ausblenden. Die Tooltips benennen weiterhin jede Aktion.

Der aktuelle Dongle-Modus bleibt als farbiges Feld in der Statusleiste sichtbar: Gelb/Grün/Blau/Rot wie in der Berechtigungsspalte. Die Auswahlwarnungen für OEM/Werk bleiben bestehen.

## Parameteranzeige und Dongle-Stufe

**Allgemein → Alle Parameter bis zur gewählten Dongle-Stufe anzeigen** ist ein eigener Schalter. Eingeschaltet (Standard): Definitionen ohne entschlüsselte Werte erscheinen als **Nicht gelesen**, grau und schreibgeschützt. Ausgeschaltet: Nur entschlüsselte Werte aus Datei oder Controller-Lesevorgang werden angezeigt. Beide Varianten beachten die gewählte Dongle-Stufe. Graue Einträge sind keine erfundenen Werte und gelangen nicht in den Controller-Schreibplan.

Standard ist **Händler (1)**. Therapeut (0) kann ebenfalls als Standard gespeichert werden. OEM (2) und Werk (3) erfordern eine Warnung im Dongle-Menü und gelten nur für die aktuelle App-Sitzung. Die Farben bleiben Gelb, Grün, Blau und Rot für Stufe 0–3. Geänderte gelesene Rohwerte bleiben rot und fett.
