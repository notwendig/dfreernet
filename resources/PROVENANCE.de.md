# Eingebettete Laufzeitdaten

[English](PROVENANCE.md) | [Deutsch](PROVENANCE.de.md)

Am 10.11.2026 einmalig aus dem lokal geprüften Schema-33-Katalog und den Ressourcentabellen übernommen. Die native Anwendung und der normale Build lesen keine Originalinstallation. Dies sind abgeleitete Referenzdaten, kein neu verfasster oder unabhängig lizenzierter Ersatzkatalog. Die GPL-3.0-only-Freigabe für eigenen Code umfasst diese Daten nicht; bestehende Weitergabebeschränkungen bleiben maßgeblich. Siehe [Geltungsbereich](../SCOPE.de.md).

SHA256 der Quell-EXE: `d5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec`. SHA256 des Katalogklartexts: `30bbe3a7edc45b1ec7975544254bbec6853199a899f46144b33eeebe946723cc` (4.814.680 Byte). Unabhängige Parameter-/Speicher-/Wertreferenzen bleiben für Vergleichstests erhalten.

Die Laufzeitdaten liegen jetzt in `dfreernet.sqlite3`. Parameter, Texte, Metadaten, Speicherzuordnungen, Editoren, Menüs und Ressourcentexte werden aus strukturierten Tabellen gelesen. `asset_blobs` enthält zusätzlich die ursprüngliche komprimierte Katalogreferenz, die Snapshotvorlage mit opaken verschlüsselten Metadaten und die Standard-Blowfish-Konstanten. Die Vorlage erzeugt keine aktuelle Controlleridentität. Herkunftshinweise stehen auch in der Tabelle `provenance`.

`SHA256SUMS` legt die Datenbank fest. CMake prüft sie und bettet sie komprimiert ein; EXE, DLL oder verschlüsselte RND werden nicht eingebettet. Der optionale ursprüngliche Entwicklungsimporter ist `scripts/import_standalone_resources.cpp`. Er wird von Build und Anwendung nicht aufgerufen. Tabellenexport und SQLite-Erzeugung sind separate Entwicklungswerkzeuge.
