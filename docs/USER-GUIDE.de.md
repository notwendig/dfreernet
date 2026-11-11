# dfreernet — Bedienung

[English](USER-GUIDE.md) | [Deutsch](USER-GUIDE.de.md)

**Dongle free R-Net Programmer**, experimentelle Qt-6-Anwendung für Linux.

**Rudimentär auf Permobil T5 VS getestet.** Die vorhandenen Versuche belegen keine allgemeine Kompatibilität und ersetzen keine vollständige Erprobung. Andere Modelle und Firmwarestände sind nicht bestätigt.

## Voraussetzungen und Start

Linux mit Qt 6.10 oder neuer und einer passenden SocketCAN-Schnittstelle. Die Anwendung richtet den CAN-Adapter nicht selbst ein. Schnittstelle, Verkabelung und Bitrate müssen zur tatsächlichen Steuerung passen. Auf dem bisherigen Entwicklungsaufbau wurde `can0` mit 125 kbit/s verwendet; das ist keine automatische Vorgabe für andere Systeme.

Der geprüfte Katalog, Oberflächentexte und die Snapshotvorlage sind eingebettet. Originalinstallation, EXE, RND, Wine, Dongle und FTDI-/CUSBMsg-DLLs werden nicht benötigt. Ein kopiertes eigenständiges Programmverzeichnis über `bin/dfreernet` starten und die Controller-Hilfsprogramme daneben erhalten. Installiertes Qt ab 6.10 einschließlich Sql-Modul/QSQLITE-Treiber und zlib bleiben erforderlich.

```bash
scripts/run.sh
```

Das modale Warnfenster erscheint bei jedem Start. „Verstanden – Fortfahren“ öffnet die Oberfläche, „Beenden“ und Escape schließen den Start. Das Fenster öffnet standardmäßig auf dem zweiten Monitor. Im Menü „Sprache“ kann zwischen US-Englisch und Deutsch gewechselt werden.

## Bearbeiten → Einstellungen

Vier vertikale Reiter bündeln die dauerhaft gespeicherten Einstellungen:

| Reiter | Inhalt |
| --- | --- |
| Allgemein | Sprache, Standard-Dongle-Berechtigung, Rückgängig-Schritte |
| CAN | Schnittstelle, Monitorzeit, Herz-Anzeige und Empfangszeitlimit |
| Dateien und Ordner | Arbeits-, Sicherungs-, Export- und Protokollordner |
| Darstellung | Monitor, Maximierung, Detailbereich, Icon- und Schriftgröße |

„Anwenden“ übernimmt gültige Werte, „OK“ übernimmt und schließt, „Abbrechen“ verwirft noch nicht übernommene Eingaben. Der Protokollordner wirkt beim nächsten Start. Explizite Startparameter haben Vorrang. Die gespeicherte CAN-Schnittstelle und die Sicherungsordner werden bei Controlleraktionen ohne erneute Auswahl verwendet. Sicherheitsbestätigungen bleiben erhalten; beim Öffnen oder Speichern einer Datei wird weiterhin ein Dateiname ausgewählt.

## Konfiguration lesen

Den Rollstuhl abstellen; während des Zugriffs darf der Fahrbetrieb nicht benötigt werden. „Controller lesen“ wählen und den stationären Zustand bestätigen. Auch Lesen kann einen Programmierzugang belegen und den Fahrbetrieb vorübergehend blockieren.

Empfangene Dateien werden in einem neuen eindeutigen Unterordner des Sicherungsordners gespeichert. Die Oberfläche öffnet bevorzugt die Konfiguration des verifizierten Repository-Knotens 2. Unterstützte Parameter erscheinen als Werte; unbekannte Dateiversionen bleiben als Rohdaten erhalten und werden nicht als bekannte Parameter ausgegeben.

## Parameter bearbeiten und Dateien speichern

Suche und Gruppenbaum begrenzen die Anzeige. Doppelklick oder F2 in der Rohwertspalte öffnet einen zum Parameter passenden Editor: Ja/Nein, Auswahl, Ganzzahl oder Text. Grenzen, Schrittweite und Textkodierung werden geprüft. Ungeklärte Sondertypen bleiben schreibgeschützt. Escape verwirft die laufende Eingabe.

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

Das betrifft den Linux-Rechner. Der Sleep Timer des Rollstuhls bleibt unverändert. Eine temporäre Wachhaltewirkung durch Lichtbefehle ist am T5 VS noch nicht bestätigt; eine gespeicherte Live-Signal-Einstellung kann nach der allgemeinen Startwarnung einen Warnblinker-Zyklus fortsetzen; standardmäßig ist sie ausgeschaltet.

## CAN-Herz und Identifikation

Rechts in der Statusleiste pulsiert das Herz rot, wenn aktuelle vollständige Heartbeat-Nachrichten empfangen werden. Ohne aktuellen Empfang ist es grau und still. Eine ausgeschaltete Schnittstelle steht ausdrücklich im Tooltip. Nach USB-/Schnittstellenwechsel bindet sich der Empfänger automatisch neu; er konfiguriert den Adapter nicht und sendet keine CAN-Nachrichten. Die Anzeige hört ausschließlich mit und sendet keine CAN-Nachrichten. Sie bestätigt weder Fahrbereitschaft noch Schreibberechtigung. Ein graues Herz kann auch durch eine falsche Schnittstelle oder einen auf diesem System abweichenden Heartbeat entstehen.

Seriennummernabfrage und CAN-Monitor verwenden die gespeicherte Schnittstelle. Eine Seriennummer ist nicht automatisch ein zuverlässig ermittelter Modulname.

## Bei Problemen

Schnittstelle und Ordner zuerst in Einstellungen prüfen. Sicherung, Protokoll und verwendete Programmversion aufbewahren. Nach einer unbestätigten Übertragung keinen Schreibversuch wiederholen, bevor der tatsächliche Stand gelesen ist. Ein vollständiger Wiederherstellungsablauf ist nicht für alle Steuerungsvarianten nachgewiesen.

## Optionales Live-Signal: Warnblinker

Unter **Bearbeiten → Einstellungen** bietet die CAN-Seite **Live-Signal: regelmäßiger Warnblinker** (Ja/Nein), Intervall (Standard **300 Sekunden**) und Blinkdauer (Standard **60 Sekunden**). Die Dauer muss kürzer als das Intervall sein. Der CAN-Menüpunkt startet oder beendet den Zyklus direkt. Die gespeicherte Aktivierung setzt ihn beim nächsten Start nach der allgemeinen Startwarnung fort; eine zusätzliche Lichttest-Warnung oder Bestätigung entfällt.

Bei vorhandenem aktuellem Heartbeat startet ein Zyklus sofort. Das Lichtsymbol ist während eines Blinkimpulses gelb und in den Pausen oder beim Warten grau. Der Tooltip zeigt die Zeiten oder den Grund für das Warten. Sobald Heartbeat zurückkehrt oder der Controllerzugriff endet, beginnt ein fälliger Zyklus nach ungefähr 100 ms statt erst nach einem weiteren vollständigen Intervall. Das nächste Intervall zählt ab dem tatsächlich gesendeten ersten Umschaltbefehl. Neue Zyklen warten während eines Controllerzugriffs. Deaktivieren oder Schließen der App versucht den zweiten Umschaltbefehl eines laufenden Paares. Schnittstellenwechsel oder Sendefehler stoppen den Zyklus; tatsächliche Sendefehler werden nicht wiederholt. Fehler erscheinen im Tooltip des Lichtsymbols und im Protokoll.

Die Befehle schalten Lampen um und liefern keine Rückmeldung zum Lampenzustand. Der Benutzer bestätigte Blinken in der aktuellen T5-VS-Sitzung; das Protokoll dokumentiert beide Umschaltbefehle mit genau 60 Sekunden Abstand. Lampenzustands-Rückmeldung und allgemeine Hardwareprüfung stehen weiterhin aus. Der Rollstuhl schaltete sich trotz des früheren Zyklus später aus. Dies ist daher **keine zuverlässige Schlafsperre**. Controllerparameter bleiben unverändert. CLI-Start ist mit `scripts/run.sh --hazard-blink-test` möglich; kein Heartbeat-Zeitlimit beendet einen wartenden Zyklus.

Unter **Darstellung → Werkzeugleiste: nur Symbole** lassen sich die Beschriftungen ausblenden. Die Tooltips benennen weiterhin jede Aktion.

Der aktuelle Dongle-Modus bleibt als farbiges Feld in der Statusleiste sichtbar: Gelb/Grün/Blau/Rot wie in der Berechtigungsspalte. Die Auswahlwarnungen für OEM/Werk bleiben bestehen.

## Parameteranzeige und Dongle-Stufe

**Allgemein → Alle Parameter bis zur gewählten Dongle-Stufe anzeigen** ist ein eigener Schalter. Eingeschaltet (Standard): Definitionen ohne entschlüsselte Werte erscheinen als **Nicht gelesen**, grau und schreibgeschützt. Ausgeschaltet: Nur entschlüsselte Werte aus Datei oder Controller-Lesevorgang werden angezeigt. Beide Varianten beachten die gewählte Dongle-Stufe. Graue Einträge sind keine erfundenen Werte und gelangen nicht in den Controller-Schreibplan.

Standard ist **Händler (1)**. Therapeut (0) kann ebenfalls als Standard gespeichert werden. OEM (2) und Werk (3) erfordern eine Warnung im Dongle-Menü und gelten nur für die aktuelle App-Sitzung. Die Farben bleiben Gelb, Grün, Blau und Rot für Stufe 0–3. Geänderte gelesene Rohwerte bleiben rot und fett.
