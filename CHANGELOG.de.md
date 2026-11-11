# Änderungen

[English](CHANGELOG.md) | [Deutsch](CHANGELOG.de.md)

## Unveröffentlicht

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
- Abhängigkeit von der Originalinstallation für nativen Start, Menüs, Katalog, Einstellungen und Controller-Hilfsprogramme entfernt; geprüfte Projektressourcen eingebettet und verschiebbares lokales Programmverzeichnis ergänzt.
- Native Anwendung und Programmdatei in **dfreernet — Dongle free R-Net Programmer** umbenannt.
- Kurze, eindeutige Titelleiste: `dfreernet` beziehungsweise `Dateiname — dfreernet`.
- Vorhandene Qt-Einstellungen beim ersten Start unter dem neuen Namen erhalten.
- Aktuelle README statt chronologischer Notizen im Hauptverzeichnis; bisherige Berichte unter `docs/history/` erhalten.
- README, Bedienung, Entwicklung, Teststand und Änderungen auf Englisch und Deutsch dokumentiert, einschließlich der rudimentären Erprobung am **Permobil T5 VS**.
- Dauerhafte Einstellungen in vertikalen Reitern und passive pulsierende CAN-Herz-Anzeige rechts in der Statusleiste ergänzt.
- Startwarnung, typisierte Rohwerteditoren, Berechtigungsanzeige, Sicherungen und begrenzte Schreib-/Rückleseprüfung aus dem bisherigen Entwicklungsstand erhalten.

Dies ist eine Entwicklungsversion, keine allgemein validierte Veröffentlichung.
