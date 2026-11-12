# Änderungen

[English](CHANGELOG.md) | [Deutsch](CHANGELOG.de.md)

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
