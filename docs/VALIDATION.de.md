# Teststand und bekannte Grenzen

[English](VALIDATION.md) | [Deutsch](VALIDATION.de.md)

Stand: 10. November 2026. Anwendung: dfreernet 0.1.0, natives Linux / Qt 6.

## Hardwareumfang

**Rudimentär auf Permobil T5 VS getestet**, entsprechend der Modellangabe des Benutzers. Vorhandene Protokolle dokumentieren Entwicklungsversuche am angeschlossenen R-Net-System; sie bestätigen Modell, Hardwareversion oder Firmwarebestand nicht unabhängig. Eine strukturierte Gesamtabnahme, Langzeiterprobung oder allgemeine Kompatibilitätsprüfung wurde nicht abgeschlossen. Eine Prüfung des Fahrverhaltens wird nicht behauptet.

| Bereich | Vorhandener Nachweis | Umfang |
| --- | --- | --- |
| Konfiguration lesen | `analysis/pop-reply-readback-result.json` und zugehöriger Snapshot | 23 Dateien / 3.581 Byte; 23 exakte Vergleiche mit CAN-Nutzdaten am Entwicklungssystem |
| OBP-Parameter | Frühere Änderung von PID 68 von 0 auf 1 samt Rücklesen; Benutzer meldete funktionierendes OBP | Eine ausdrücklich autorisierte Parameteränderung |
| Textänderungen | `analysis/write-readback-fix-findings.md`, `analysis/pop-reply-fix-findings.md` | Vom Benutzer ausgelöste Profil-/Achsnamenänderungen; nachfolgendes Rücklesen bestätigte Werte ohne erneutes Schreiben |
| Seriennummern | `analysis/real-can-serial-findings.md` | Antworten und Originalformat geprüft; Modultypnamen noch unvollständig |
| Einstellungen und Heartbeat | `analysis/settings-heartbeat-tests.log`, `analysis/settings-heartbeat-vcan.log` | Persistenz, EN/DE-Oberfläche, Filterung, Zeitlimit, null Sendeframes des Indikators im privaten vcan-Test |
| Live-Signal und ungelesene Definitionen | `analysis/live-signal-settings-tests.log`, `analysis/live-signal-settings-vcan.log` | 17/17 Offline-Ziele; Ja/Nein, Intervall und Dauer, kein Senden vor Heartbeat, graue schreibgeschützte Definitionen, Stufenfilter und Warnungen für erhöhte Sitzungsrechte; virtuelles CAN-Befehlspaar |

Diese lokalen Nachweise sind private Entwicklungsartefakte, kein weitergabefähiges Testdatenpaket. Spätere Namens- und Dokumentationsänderungen stellen keine zusätzliche Hardwareerprobung dar.

Eine spätere passive CAN-Messung über 31 Minuten am 11. November 2026 erfasste 31 Warnblinker-Umschaltpaare. Der Rollstuhl schaltete 1800,328 Sekunden nach dem letzten nicht neutralen Joystick-Frame ab, obwohl das letzte Paar erst 37 Sekunden zuvor endete. Die zyklischen Modul-Heartbeats liefen bis zum Abschalten. Der Lichtzyklus verhinderte das automatische Abschalten in dieser Sitzung nicht. Die Rohdaten bleiben im privaten lokalen Analyseordner.


Ein separater Einschaltvergleich am 11. November 2026: Ein einziger, ausdrücklich autorisierter `0x00C`-Frame ließ das Display kurz aufleuchten; in den folgenden sechs Sekunden antwortete kein CAN-Modul, und das Display ging wieder aus. Beim anschließenden Einschalten über den Ein/Aus-Taster blieb das Display an. Der passive Mitschnitt begann mit `0x00C`, nach etwa 21 ms `0x00E` (acht Byte), dann Erkennungsframes und zyklischem Verkehr. Damit ist `0x00C` allein kein bestätigter Fern-Einschaltweg. Die Rohdaten mit Modulkennung bleiben privat unter `analysis/`. Ein späterer Einzelversuch mit `0x00C` und 23 ms danach `0x00E` mit der zuvor beobachteten Kennung bewirkte ebenfalls nur ein kurzes Aufleuchten; während acht Sekunden kamen keine CAN-Antworten. Auch dieses Paar ist kein bestätigter Fern-Einschaltweg. Drei anschließend einzeln gesendete `0x00C`-Frames im Abstand von 250 ms bewirkten ebenfalls nur ein kurzes Aufleuchten ohne CAN-Antwort; sie hielten den Rollstuhl nicht an.

## Automatisierte Prüfungen

`scripts/test.sh` baut die Anwendung, startet die Offline-CTest-Suite, prüft beim Build die festen Prüfsummen eingebetteter Ressourcen und native Laufzeitabhängigkeiten. Mit lokalen privaten Referenzdaten umfasst die Suite 17 Ziele; der Git-Quelltextstand führt die referenzdatenfreien Tests aus: Dateierhaltung, Katalogdekodierung, Werteprüfung, begrenzte Schreiblogik, Mitschnittprüfungen, Übersetzungen, Startzustimmung, Einstellungen und Qt-Bedienung. Diese Tests senden nicht auf einen physischen CAN-Bus.

Der optionale Heartbeat-Integrationstest verwendet einen neuen isolierten Netzwerk-Namespace mit virtuellem CAN. Er prüft, dass fremde/unvollständige Frames den Indikator nicht aktivieren, ein vollständiger Frame mit ID `0x00E` ihn aktiviert, veralteter Empfang grau wird und der Indikator null Frames sendet. Benötigt Linux-Namespace-/vcan-Unterstützung; getrennt vom Standard-CTest-Lauf.

## Bekannte Lücken

- Die vollständige OEM-Rekonstruktion ist nicht abgeschlossen. Nicht unterstützte Aktionen bleiben gesperrt.
- Weitere Repository-Dateiversionen und Parameterumrechnungen benötigen geprüfte Zuordnungen. Unbekannte Nutzdaten bleiben erhalten, ohne Deutungen zu erraten.
- Die Erzeugung verschlüsselter Dokumentmetadaten ist unvollständig. Konfigurationssnapshots können Metadaten der Original-Defaultvorlage enthalten; aktuelle Modulseriennummern sind gesondert zu betrachten.
- Der Schreiber ist auf unterstützte Zuordnungen und den geprüften Repository-Pfad von Knoten 2 begrenzt. Andere Zielmodule und beliebige Firmware werden nicht daraus abgeleitet.
- Mehrere Dateien können nur teilweise geschrieben sein. Commit-Bestätigung und verifiziertes Rücklesen sind getrennte Zustände; keine automatische Schreibwiederholung.
- Eine zuverlässige Zuordnung der Modulnamen/-typen ist unvollständig.
- Das passive Herz überwacht den beobachteten Acht-Byte-Heartbeat mit ID `0x00E`. Es belegt weder den Zustand des gesamten Systems noch eine universelle Heartbeat-Abdeckung.
- Die native Anwendung bettet abgeleitete Referenzressourcen ein. Deren Herkunft und Weitergabebeschränkungen bleiben bestehen; es handelt sich nicht um einen unabhängig erstellten Ersatzkatalog. Opaque Dokumentmetadaten stammen weiterhin aus der eingebetteten Referenzvorlage.

## Nachvollziehbare Fehlermeldung

Programmversion und Quelltextprüfsumme, Linux-/Qt-Versionen, Adapter-/Schnittstellenkonfiguration, genaue Controller-/Modulfirmware soweit bekannt sowie Vorgang, Erwartung und Ergebnis festhalten. Vorher-/Ziel-/Rücklesedateien und Vorgangsprotokoll erhalten. Persönliche Angaben und Seriennummern vor einer Weitergabe entfernen. Einen unbestätigten Schreibvorgang nicht allein zur Reproduktion wiederholen.
