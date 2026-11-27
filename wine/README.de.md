# Wine / ftd2xx.dll

[English](README.md) | [Deutsch](README.de.md)

Eigenständiges Unterprojekt für den vorhandenen 32-Bit-OEM-Programmer:

```text
OEM-Programmer / IRConfigurator (Wine)
  -> Interface-Code (statisch gelinkt / DongleInterface.dll)
  -> ftd2xx.dll
       CRawMsg -> CUSBMsg: lokale Dongle-Kommandos und Antworten
       CRawMsg -> CCANMsg: UDP auf 127.0.0.1:39000
                            -> dfreernet-wine-can-proxy -> can0
       CAN RX <- Proxy -> Raw24 mit Dongle-RX-Prüfsumme -> FT_Read
```

Die Umgebungsvariable `DONGLE` bestimmt das simulierte Profil:

| Wert | Zugriffsstufe |
| --- | --- |
| `Dealer` | 1 |
| `OEM` | 2 |
| `Factory` | 3 |

Ohne `DONGLE` gilt weiterhin `OEM`. Die Schreibweise ist verbindlich; leere
oder andere Werte werden vor dem Öffnen des Transports abgewiesen.
Die DLL liest die Variable einmal beim Initialisieren ein. Eine laufende
Programmer-Instanz muss zum Wechseln des Profils neu gestartet werden.

Das virtuelle EEPROM-Profil in `common/DongleIdentity.hpp` liefert die gewählte
Stufe über USB `07/00`, Adresse `0x86`; die passende Identitäts-CRC wird berechnet
und über Adresse `0x8B` ausgegeben. Die Kennung liegt im Speicher der simulierten
DLL und wird bei jedem Start neu initialisiert.

Die DLL emuliert das FTDI-/Dongle-Interface. CAN-/POP-Nutzdaten werden zum Bus
weitergereicht; es wird kein `rollstuhl.emu` gestartet. Der aus der Referenz
übernommene, inaktive Legacy-Replay-/Block-Code wird nicht aktiviert.
Die DLL meldet ein angeschlossenes System erst nach tatsächlichem CAN-Empfang.
Der USB-Status meldet als Repository-Knoten `0xFF` (unbekannt). Dadurch sucht
`DongleInterface.dll` den Repository-Server selbst mit POP `40/80` auf den
Knoten 0..14. Der aus einer einzelnen Sitzung übernommene Wert `0` würde den
Programmer auf Knoten 0 festlegen und bei anderen Controllern in der
Initialisierung festhalten (wiederholtes `20/80=0x1F`, POP-Abbruch `0x20`).

Für die physische Verbindung gilt die [Anschlussanleitung](../README.de.md#rollstuhl-mit-pc-oder-raspberry-pi-verbinden), einschließlich der erforderlichen galvanischen Trennung.

Für native lokale IR-Menübearbeitung siehe [qt_iredit](../qt_iredit/README.de.md). Der Editor öffnet/speichert Pronto-RnIR3-Menüs und übernimmt eigene Pronto-Hex-Codes ohne Wine oder CAN-Proxy.

## Bauen und offline prüfen

Zusätzlich zum nativen C++17-Compiler: CMake, Ninja, MinGW-w64 i686 und Wine.
Der normale dfreernet-Build baut den nativen Proxy und registriert den
hardwarefreien Konvertierungstest. Für die Windows-DLL:

```bash
wine/build.sh
python3 wine/tests/offline.py
scripts/test.sh
```

Ausgaben liegen ausschließlich unter `build/wine/`. `DFREERNET_WINE_BUILD` kann
einen anderen Build-Pfad festlegen. Beim Offline-Test mit anderem Build-Pfad
ist dessen `mingw32`-Verzeichnis als Argument zu übergeben.

Der Wine-Test lädt die echte PE32-DLL, beantwortet deren UDP-Handshake selbst
und prüft lokale USB-Antworten, fragmentierte FT_Write-Aufrufe, fehlerhafte
Prüfsummen/DLC, Standard-/Extended-/RTR-Frames, DLC 0, Dreier-Bursts und CAN-RX.
Er prüft außerdem alle drei `DONGLE`-Profile, den OEM-Standard ohne Variable
und die Ablehnung ungültiger Werte ohne Transportstart. Er öffnet keinen
CAN-Socket. Wine verwendet das vorhandene `WINEPREFIX`.
Private-fixture-Tests des Hauptprojekts werden nur bei vorhandenen Fixtures
registriert; das Wine-Unterprojekt benötigt diese nicht.

## Programmer starten

Nur bei ausdrücklich freigegebenem Betrieb am stationären Rollstuhl:

```bash
wine/run.sh --prepare-only  # nur private Laufzeitkopie vorbereiten
DONGLE=OEM wine/run.sh      # Programmer und Proxy für can0 starten
DONGLE=Factory wine/run.sh  # alternativ: Werk/Factory
DONGLE=Dealer wine/run.sh   # alternativ: Dealer
```

Standard-Original:
`~/.wine/drive_c/Program Files (x86)/PG Drives Technology/R-net Programmer OEM Generic/RNet Programmer6 OEM.exe`.
`--programmer /pfad/zur/exe` und `--interface canN` überschreiben diese Auswahl.
Die bestehende CAN-Schnittstelle muss bereits eingerichtet und aktiv sein
(R-net: 125000 bit/s). Das Startskript konfiguriert das Interface nicht.

Das Skript kopiert die Programmer-Laufzeit einschließlich ihrer Ressourcen nach
`build/wine/runtime`, ohne alte Logs und ZIP-Archive. Es ersetzt die DLL nur in
dieser Kopie und erzwingt dort `WINEDLLOVERRIDES=ftd2xx=n`. Die installierten
Originale bleiben erhalten. Kopierte proprietäre Dateien bleiben im ignorierten
Build-Verzeichnis und dürfen nicht als Projektressourcen eingecheckt werden.

Das Skript läuft im Vordergrund. Schließen des Programmers oder Strg+C beendet
den zugehörigen Proxy. Andere Wine-/Emulator-Prozesse werden nicht beendet.
Jeder Lauf erhält eigene Logs unter `build/wine/logs/<Zeitstempel>/`:
`programmer.log`, `proxy.log` und (sobald die DLL geladen wird) `ftd2xx.log`.

Für einen grafischen OEM-Starter kann der Desktop-Menüeintrag
`python3 /absoluter/pfad/zu/dfreernet/wine/start_oem.py` aufrufen
(`Terminal=false`). Dieser setzt `DONGLE=OEM` und das normale `~/.wine`-Präfix,
startet die geprüfte OEM-Wine-Kopie samt Proxy und hält doppelte Aufrufe dieses
Starters von derselben Sitzung fern. Startfehler erscheinen bei vorhandenem
Zenity als Dialog; das Startprotokoll liegt in `build/wine/oem-launcher.log`.
Ein Direktstart der installierten EXE umgeht weiterhin diesen Startpfad.

`wine/start_ir.py` ist der entsprechende grafische Starter für die originale
`IRConfigurator.exe`: OEM-Dongle, dieselbe vorbereitete Laufzeit und ein eigener
Proxy-Lebenszyklus. Das Startprotokoll heißt `build/wine/ir-launcher.log`.
OEM-Programmer und IRConfigurator aus diesen Startern werden nacheinander
verwendet; vor dem Wechsel das laufende Programm schließen. Ein bereits aktiver
Proxy wird vor dem Überschreiben der Laufzeitdateien erkannt.

Der Programmer kann selbst CAN-Schreibbefehle auslösen. Der Proxy übernimmt
keine Beschränkung auf Lesezugriffe. Die bestehende Power-Off-Sperre bleibt:
Nach Empfang von Standard-ID `002` als RTR bleibt CAN-TX bis zum Neustart des
Proxys gesperrt; `00C` hebt die Sperre nicht auf.

## Lokales Proxy-Protokoll

DLL und Proxy werden gemeinsam gebaut. Das lokale UDP-Format `RNC2` besteht aus
20 Bytes: Magic (0..3), Typ (4: Hello=1, TX=2, RX=3, Ack=4), DLC (5), CAN-ID
little-endian (6..9), Daten (10..17), Flags (18: Bit 0 Extended, Bit 1 RTR),
reserviert (19, null). Dadurch bleiben auch Extended-IDs unter `0x800` und RTR
erhalten. Port: `39000 + N` für `canN`, N=0..999. Das frühere `RNC1`-Format der
Referenz ist nicht kompatibel; dessen DLL und Proxy nicht mit diesem Paar mischen.

## Herkunft

Die Dateien in `ftd2xx/`, `common/`, die Toolchain und der Ausgangsstand des
Proxys wurden aus dem vom Benutzer benannten lokalen Projekt
`/home/juergen/Projects/RNet-Dongle-Emu` übernommen, Commit
`742913ccc668b1b32093a266e657f60b7c9f1cf9` (Quellstand ohne lokale Änderungen).
Die Anpassungen betreffen typisierte CUSBMsg-/CCANMsg-Verarbeitung, DLC-Prüfung,
EXT/RTR-Transport, lokale Peer-Prüfung, Filterstatus, statische MinGW-Laufzeit,
Tests und den separaten Startpfad. Dies dokumentiert die Herkunft und trifft
keine zusätzliche Lizenzzusage für übernommene oder proprietäre Bestandteile.

## Download-Frist unter Wine

Der Startpfad korrigiert die automatisch berechnete Download-Frist sowohl in
der bekannten `DongleInterface.dll` (IRConfigurator) als auch in einer separaten
OEM-/Factory-Wine-EXE. Der OEM-Programmer enthält den Download-Code statisch
und lädt `DongleInterface.dll` nicht; eine reine DLL-Korrektur reicht dort
nicht aus. Die festen
200 ms werden in beiden Download-Zeitmodellen auf 5000 ms erhöht; die originalen
3 bzw. 5 ms pro Nutzbyte bleiben erhalten. Für 1070 Bytes beträgt die kurze
Frist dadurch 8210 statt 3410 ms. Uploads und die Power-Off-Sendesperre werden
nicht verändert. Die installierte Original-DLL bleibt erhalten.

Unterstützte Original-SHA256:
`b45ad5e2579b4811cd6c48fd89d0e333c63fafd28d2c6ebf56b7144c35f7e829`.
Ergebnis-SHA256:
`55c60b4a0c32ec24e8c0d246099dcdcab1e991f6695ba595178c0dcc080300f1`.
Andere DLL-Versionen werden vor dem Staging abgewiesen. Geändert werden nur
die beiden Timeout-Konstanten im automatischen Download-Pfad; die digitale
Signatur der lokalen Kopie ist dadurch nicht mehr gültig. Proprietäre
Binärdateien werden nicht mitgeliefert.

Die unveränderten OEM-/Factory-Stagingkopien bleiben erhalten. Gestartet wird
`RNet Programmer6 OEM Wine.exe` bzw. `RNet Programmer6 Factory Wine.exe`.
Nur die beiden Download-Konstanten bei Datei-Offsets `0x24A1E` und `0x24A2A`
werden zusätzlich geändert; die benachbarten Upload-Fristen bleiben unverändert.
Unterstützte EXE-Ausgangsstände sind die oben bekannte Original-OEM-EXE und
die verifizierte Zwei-Byte-Factory-Kopie. Die Ausgabeprüfsummen lauten:

- OEM Wine: `9040c12d005b2917f1ea6eccf595cccba7fb6dcf1a97f5f31b5a2e27c01b51e3`
- Factory Wine: `9428cf1aa050480232e1e61aef8443c9b1a855563ffdbc105bb3567ebd100ac4`

Offline wurde der echte Download-Aufruf der Original-DLL mit synthetischen
1070 Bytes und verzögerten UDP-Segmentbestätigungen geprüft: Die Originalfrist
brach nach rund 3420 ms vorzeitig ab. Die korrigierte Kopie übertrug alle
134 Segmente inklusive Abschlusskennzeichen und unabhängig geprüfter CRC in
rund 4400 ms. Zusätzlich wurde die tatsächlich korrigierte OEM-Wine-EXE im
isolierten Profil geprüft: 1070/1070 Bytes, 134 Segmente, Abschlusskennzeichen
und unabhängig bestätigte CRC nach 4458 ms. Eine reine DLL-Korrektur behob den
OEM-Abbruch nicht, weil die EXE den Code statisch enthält. Eine bereits
beschädigte Datei im Controller wird durch das Staging allein nicht repariert.

```bash
python3 wine/tests/download_timeout_patch_test.py \
  --original '/pfad/zur/originalen/DongleInterface.dll' \
  --programmer '/pfad/zur/originalen/RNet Programmer6 OEM.exe'
DONGLE=OEM wine/run.sh --prepare-only
```

Der Startpfad kopiert außerdem vorhandene `libgcc_s_dw2-1.dll` und
`libwinpthread-1.dll` aus `/usr/i686-w64-mingw32/sys-root/mingw/bin` neben die
Laufzeitkopie. Diese Abhängigkeiten benötigt das lokal installierte 32-Bit-
Wine-Mono für `IRConfigurator.exe`; ohne libgcc endete der Teststart mit Code
255. Ein anderer Bibliotheksordner lässt sich über
`DFREERNET_MINGW32_RUNTIME_DIR` angeben. Das Wine-Präfix wird nicht verändert.
Nach Ergänzung der Bibliotheken erreichte der isolierte IRConfigurator-Test
die Geräteauswahl. Die vollständige IR-Kommunikation wurde damit nicht geprüft.

## Separate EXE-Kopie für Factory

Die bekannte OEM-EXE begrenzt die wirksame Stufe auf
`min(Anwendungsstufe 2, Dongle-Stufe)`. Die Option `--factory-exe` erstellt
zusätzlich `build/wine/runtime/RNet Programmer6 Factory.exe` mit Anwendungsstufe
`3`. Damit bleibt die wirksame Stufe vom gewählten Dongle abhängig: Dealer=1,
OEM=2, Factory=3.

```bash
DONGLE=Factory wine/run.sh --factory-exe --prepare-only
DONGLE=Factory wine/run.sh --factory-exe
```

`wine/patch_factory.py QUELLE.exe ZIEL.exe` erzeugt die Kopie auch separat.
Der Patcher akzeptiert ausschließlich den geprüften SHA256-Stand
`d5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec`.
Er ändert genau zwei Bytes: Datei-Offset `0x18B2F5`, Immediate der Instruktion
bei VA `0x0058BEEF` (`mov dword ptr [0x006CE33C], 2` wird zu `3`), und
Datei-Offset `0x1D7AB5`: `bVistaStyle` beim MFC-Dateidialog von TRUE auf FALSE.
Dadurch verwendet die Kopie den klassischen Dateidialog.
Die Ziel-Prüfsumme lautet
`919deed14f206ddc7d23ad2429b0070f523fa895cfcdd8f0716ba1050a515751`.
Eine vorhandene, exakt bekannte ältere Ein-Byte-Patchkopie wird unter
`.exe.v1.bak` gesichert und aktualisiert; unbekannte Zieldateien werden abgelehnt.
Die Änderung macht die ursprüngliche digitale Signatur ungültig; diese Kopie
ist eine lokale Modifikation. Sie ergänzt keine in der OEM-Version fehlenden
Factory-Funktionen. Beide EXE-Dateien bleiben im ignorierten Build-Verzeichnis.

Die Patch-Guards laufen im normalen CTest. Gegen die lokal vorhandene EXE:

```bash
python3 wine/tests/factory_patch_test.py --original 'build/wine/runtime/RNet Programmer6 OEM.exe'
wine build/wine/mingw32/wine_factory_limit_test.exe \
  'Z:\home\juergen\Projects\dfreernet\build\wine\runtime\RNet Programmer6 OEM.exe' \
  'Z:\home\juergen\Projects\dfreernet\build\wine\runtime\RNet Programmer6 Factory.exe'
```

Der Windows-Prüfer führt nur die extrahierten Initialisierungs- und
Stufenbegrenzungs-Instruktionen aus (die globale Adresse wird dabei auf
Testspeicher umgesetzt). Er lädt den Programmer nicht und öffnet
keinen CAN-Zugang. Eine vollständige Factory-Sitzung mit der Oberfläche wird
damit nicht geprüft.


### Speichertest und Wine-Registrierung

Die isolierte Stufenprüfung ist kein vollständiger Speichertest. Den gemeldeten
Nullzeigerzugriff im Wine-Dateidialog beim Speichern einer `.R-net`-Datei führte
der Benutzer auf die Wine-Registrierung zurück. Die konkrete Registry-Änderung
ist hier nicht dokumentiert; eine allgemeine Registry-Reparatur wird deshalb
nicht automatisiert. Version 1.4.1 enthält den optionalen Patcher, keine EXE.

Nach der Registry-Reparatur wurden im geklonten Wine-Profil mit der aktuellen
Zwei-Byte-Patchkopie und `DONGLE=Factory` die Anzeige „Generic Factory“, die
Anhebung einer privaten Datei, „Save As“ und das erneute Öffnen erfolgreich
geprüft. Originaldatei unverändert, kein Absturzprotokoll. Der Test nutzte nur
lokales UDP-Replay ohne SocketCAN-Proxy. Nicht alle Diagnoseabfragen waren im
Replay vorhanden; physische Controller-Schreibvorgänge wurden nicht geprüft.

Für eine gezielte Fehleraufzeichnung beim üblichen Start:

```bash
RNET_DEBUG_CRASH=1 DONGLE=Factory wine/run.sh --factory-exe
```

Die optionale DLL-Diagnose protokolliert Zugriffsverletzungen mit Registern und
Stack-Adressen unter `build/wine/logs/<Lauf>/ftd2xx.log.fault`. Sie verändert
weder die Ausnahmebehandlung noch die CAN-Nachrichten. Diese Diagnose benötigt
keinen angehängten WineDbg-Prozess. Bei einer getrennten Diagnosekopie trat
zusätzlich eine SftTree-Lizenzmeldung auf; Lizenzprüfungen werden nicht gepatcht.
