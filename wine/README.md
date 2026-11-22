# Wine / ftd2xx.dll

Eigenständiges Unterprojekt für den vorhandenen 32-Bit-OEM-Programmer:

```text
RNet Programmer6 OEM.exe (Wine)
  -> DongleInterface.dll
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

Das Profil bestimmt die Dongle-Kennung. Die Berechtigungsgrenzen der
installierten Programmer-EXE bleiben bestehen.

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

## Optionale Fehlerdiagnose

`RNET_DEBUG_CRASH=1 DONGLE=OEM wine/run.sh` zeichnet bei Zugriffsverletzungen
Register und Stack-Adressen unter `build/wine/logs/<Lauf>/ftd2xx.log.fault` auf.
Die Diagnose verändert weder die Ausnahmebehandlung noch CAN-Nachrichten und
benötigt keinen angehängten Debugger. Fehlerberichte bleiben lokale Artefakte.
