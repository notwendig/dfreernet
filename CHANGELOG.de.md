# Änderungen

[English](CHANGELOG.md) | [Deutsch](CHANGELOG.de.md)

## 1.4.1 — 22. November 2026

- `--factory-exe` erzeugt eine separate Factory-fähige Kopie des unterstützten OEM-Programmers mit SHA256-Prüfung von Quelle und Ergebnis. Originaldatei erhalten und unbekannte Zieldateien ablehnen.
- Anwendungsstufe von 2 auf 3 anheben und für Wine-Kompatibilität den klassischen MFC-Dateidialog wählen; `DONGLE=Dealer|OEM|Factory` bleibt separat wählbar.
- Offline-Patch-Prüfungen und Stufentest mit extrahierten Instruktionen ergänzen. Generic Factory, Dateianhebung, Speichern unter und erneutes Öffnen einer privaten Datei im isolierten Wine-Profil mit UDP-Replay bestätigt; kein physischer CAN-Zugriff und keine Controller-Schreibvorgänge. Proprietäre Binärdateien bleiben ausgeschlossen.

## 1.4.0 — 22. November 2026

- Optionale PE32-FTD2XX-DLL und nativen UDP-/SocketCAN-Proxy für eine vorhandene OEM-Programmer-Installation unter Wine ergänzt; CUSBMsg lokal verarbeiten und CCANMsg in beide Richtungen weiterleiten.
- Extended-CAN-IDs, RTR-Kennzeichen, Frames ohne Nutzdaten, fragmentierte Schreibaufrufe und Dreier-Bursts erhalten; dauerhafte Sendesperre nach Power-Off beibehalten.
- `DONGLE=Dealer|OEM|Factory` mit passenden EEPROM-CRCs, OEM als Standard und Ablehnung ungültiger Profile vor dem Transportstart unterstützen.
- Repository-Knoten durch den Original-Programmer suchen lassen, statt die Initialisierung auf Knoten 0 festzulegen; separate Laufzeitkopie und Protokolle pro Lauf verwenden.
- Offline-Tests für DLL, Transport und Profile sowie optionale Absturzaufzeichnung ergänzt. Proprietäre Binärdateien und Programmer-EXE-Patches sind ausgeschlossen.

## 1.3.1 — 17. November 2026

- Englische und deutsche Bildschirmfotos der aktuellen Oberfläche erneuern; Über-Dialog für vollständig sichtbaren Text vergrößern und R-Net-Datum in der Statusleiste lesbar halten.
- Qt-Designer-Uhrdialog mit laufender Host- und R-Net-Uhrzeit samt Datum, Host-zu-R-Net-Schaltfläche und bearbeitbarem R-Net-Ziel ergänzt; beide Uhren verwenden in beiden Sprachen das 24-Stunden-Format.
- Werkzeuge → Entfernung zeigt Gesamt- und Tripstrecke live an und kann beide Zähler mit den ursprünglichen CAN-Nachrichten ohne Nutzdaten zurücksetzen. Eine passende Rückmeldung bestätigt das Löschen; beliebige Streckenwerte lassen sich nicht setzen.
- Einen gemeinsamen CAN-Socket für Live-Anzeige, Uhr, Entfernung, Herz und Monitor eingeführt; ein zentraler Thread sendet Nachrichten aus einer Warteschlange.
- Überflüssige Stillstandsabfragen bei Uhr und Entfernung entfernt. Das Schreiben der Controllerkonfiguration benötigt weiterhin eine eigene Freigabe und Stillstandsbestätigung.

## 1.3.0 — 15. November 2026

- Aktuelle R-Net-Uhrzeit aus Time-of-Day-Frames in der Statusleiste anzeigen; nach Bestätigung einmalig auf die Computerzeit stellen und per CAN-Rückmeldung prüfen.
- R-Net-Datum neben der Uhrzeit anzeigen: bei Übereinstimmung mit dem Host grün, sonst rot. Uhrzeit je nach absoluter Abweichung vom Host grün, orange oder rot markieren.
- Optionale Einrichtung der CAN-Schnittstelle beim Start mit einstellbarer Bitrate nach Bestätigung des Starthinweises ergänzt.
- CAN-Zähler rechtsbündig mit Leerzeichen statt führenden Nullen anzeigen.
- CAN-Tabellenspalten per Kopfzeile auf- und absteigend sortieren; Reihenfolge beim Empfang und nach Neustart erhalten.

## 1.2.2 — 14. November 2026

- Gemeinsame CAN-Zähltabellenklasse für Mitschnitt und passive Systemtests eingeführt.
- Achtstellige Zähler, mittlere Frequenz in Hz oder Abstand in ms sowie Symbolschalter für Verlauf/Zählung und Hz/ms in beiden CAN-Bereichen ergänzt.
- Spaltenbreiten und -reihenfolge jeder CAN-Tabelle, beide Ansichtsoptionen, Größe und Position der Systemtests sowie den Gruppenbaum-Trenner speichern und wiederherstellen.

## 1.2.1 — 13. November 2026

- Fortlaufenden CAN-Mitschnitt neben Details und Vorgangsprotokoll anzeigen; Fenstergrößen getrennt einstellen.
- Verlauf und gruppierte Zählung für den passiven CAN-Mitschnitt ergänzen.
- Passive CAN-Live-Ansichten für alle zehn Menüpunkte unter Werkzeuge → Systemtests ergänzen.
- Breite des CAN-Fensters und der CAN-Tabellenspalten einstellbar machen; Fensterbreite speichern.

## 1.2.0 — 13. November 2026

- Einstellungen für bevorzugten Monitor und maximierten Start entfernt; Fensterposition, Größe und Maximierungszustand wiederherstellen und die Startwarnung auf demselben Bildschirm öffnen.
- Die unteren Fenster für Details und CAN-Mitschnitt mit sichtbarem Trenngriff höhenverstellbar gemacht; Trennerposition speichern.
- Fortlaufenden passiven CAN-Mitschnitt unter der fixierten Berechtigungsspalte ergänzt, mit automatischer Neuverbindung und begrenzter Anzeigehistorie.
- Zweisprachige Startwarnung um Verletzungsgefahr und mögliche Zerstörung von Motoren oder Steuerungselektronik ergänzt; Illustration eines kranken Rollstuhls hinzugefügt.
- Profiltabellen auf profilbezogene Parameter begrenzen; passende ungelesene Werte bleiben grau.

## 1.1.0 — 12. November 2026

- Ausstehende Spaltenlayouts aller Ansichten auch beim schnellen Ansichtswechsel erhalten.
- Das Startskript baut bei neueren Quelldateien neu, damit keine veraltete Oberfläche gestartet wird.
- Veraltete Qt-Testmakros ersetzt und die GCC-Einrückungswarnung im Katalog-Lader behoben.
- Version in Fenstertitel und Startwarnung ergänzt; Hilfe am rechten Ende der Menüleiste platziert.

## 1.0.0 — 12. November 2026

- Spalteneinstellungen für alle Tabellenansichten getrennt speichern, laden und zurücksetzen.
- Im Über-Dialog Name, Kontaktadresse, aktuelle Version und GitHub-Projektlink anzeigen.
- Veränderte Spaltenbreiten und -reihenfolgen automatisch sichern, auch beim schnellen Schließen der App.

## 0.2.2 — 12. November 2026

- Dritte Parameteransicht nach Katalogmodulen ergänzt.
- Katalogeinheit, Skala und Rohwertgrenzen nach dem Wert zusammengefasst; farbige Berechtigung rechts fixiert.
- Lange Spaltentitel zweizeilig dargestellt.

## 0.2.1 — 12. November 2026

- F1-Parameterhilfe mit eingebettetem Katalogtext gemäß der Anleitung „R-net PC Programmer“ ergänzt.
- Modul und Gruppe in der Profiltabelle anzeigen, um gleich benannte Parameter verschiedener IDs zu unterscheiden.
- Katalogeinheit, Skalierungspaar und numerische Rohwertgrenzen ohne ungeprüfte Umrechnung in der Profiltabelle anzeigen.

## 0.2.0 — 12. November 2026

- Umschaltbare Kategorien- und Profilansicht ergänzt: links Allgemein oder Profil 1–8 auswählen, rechts nur deren Parameter anzeigen.

- Sichtbar ausgeschalteten Warnblinker vor Start bestätigen, Zyklus bei verlorenem CAN-Empfang stoppen und gespeicherte Aktivierung beim Start nicht fortsetzen. Ein passiver 31-Minuten-Test zeigte, dass die Umschaltbefehle das Abschalten nicht verhindern.

- Eigenständig erstellten Code unter GPL-3.0-only gestellt; abgeleiteten Katalog und weitere Inhalte Dritter ausdrücklich ausgenommen.

## 0.1.0 — 2026-11-11

- Erster privater Git-Quelltextstand; Anwendungsversion und zweisprachige Dokumentation angeglichen.
- Intervall und Dauer des Warnblinkers in Sekunden eingestellt, Lichtzustand als Symbol angezeigt und Einstellung für reine Symbol-Werkzeugleiste ergänzt.
- Persönliche Controller-Referenzdaten, Mitschnitte, Sicherungen und generierte Builds bleiben außerhalb von Git.

### Frühere Entwicklungsarbeiten


- Permanentes Dongle-Statusfeld in Berechtigungsfarben ergänzt. Separate Lichttest-Warnung und Bestätigung entfernt; fälliges Blinken startet bei wiederkehrendem Heartbeat sofort statt nach einem weiteren Intervall. Fehlenden CAN-Empfang im Status anzeigen.
- Live-Signal konfigurierbar gemacht (Ja/Nein, Intervall, Dauer; Standard 300/60 Sekunden), erster Zyklus sofort und Bestätigung je neuer Sitzung. Ungelesene Parameter optional grau, Standard Händler und OEM-/Werk-Warnungen für die Sitzung ergänzt. Der beobachtete Zyklus verhinderte den Rollstuhl-Ruhezustand nicht zuverlässig.
- Ausdrücklich startbaren Fünf-Minuten-Warnblinkertest ergänzt: beim Start aus, paarweise Umschaltung, Heartbeat-/Busy-Prüfung, Ende beim Schließen und keine Parameteränderung; Wachhaltewirkung unbestätigt.
- Passiven Heartbeat nach USB-/Schnittstellenwechsel wiederhergestellt, ausgeschaltete Schnittstelle gemeldet und abschaltbare Linux-Schlafsperre für die App-Laufzeit ergänzt.
- Katalog- und Ressourcentabellen in SQLite gespeichert; Datei → RND-Katalog importieren… mit geprüftem Transaktionsimport in eine neue SQLite-Datenbank ergänzt.
- Geprüfte Projektressourcen für nativen Start, Menüs, Katalog, Einstellungen und Controller-Hilfsprogramme eingebettet; verschiebbares lokales Programmverzeichnis ergänzt.
- Native Anwendung und Programmdatei in **dfreernet — Dongle free R-Net Programmer** umbenannt.
- Kurze, eindeutige Titelleiste: `dfreernet` beziehungsweise `Dateiname — dfreernet`.
- Vorhandene Qt-Einstellungen beim ersten Start unter dem neuen Namen erhalten.
- Aktuelle README statt chronologischer Notizen im Hauptverzeichnis; bisherige Berichte unter `docs/history/` erhalten.
- README, Bedienung, Entwicklung, Teststand und Änderungen auf Englisch und Deutsch dokumentiert, einschließlich der rudimentären Erprobung am **Permobil T5 VS**.
- Dauerhafte Einstellungen in vertikalen Reitern und passive pulsierende CAN-Herz-Anzeige rechts in der Statusleiste ergänzt.
- Startwarnung, typisierte Rohwerteditoren, Berechtigungsanzeige, Sicherungen und begrenzte Schreib-/Rückleseprüfung aus dem bisherigen Entwicklungsstand erhalten.

Dies ist eine Entwicklungsversion, keine allgemein validierte Veröffentlichung.
