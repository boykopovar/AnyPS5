# Relinker verwenden

[**Web-Konverter**](https://boykopovar.github.io/AnyPS5/) - die Dateien bleiben auf deinem Gerät.

### Ausgabe in einen Ordner im Browser

Der Online-Konverter kann Linux-Ausgaben in unterstützten Browsern als unkomprimierten Ordner speichern. Einen vorhandenen Zielordner überschreibt er nicht. Schlägt die Konvertierung fehl oder wird sie abgebrochen, bricht er die aktive Datei ab und versucht, den neu angelegten Ausgabeordner zu entfernen. Verhindert der Browser das Aufräumen, nennt die Fehlermeldung den Ordner, der unvollständige Dateien enthalten kann.

### Stapelkonvertierung im Browser

Wähle mehrere App-ZIP-Archive aus oder ziehe sie hinein, um sie in ein ZIP-Paket zu konvertieren. Der Konverter prüft jede App einzeln und legt ihre Programmdatei, Bibliotheken und `app0/`-Dateien in einen nummerierten Ordner, sodass Archive mit gleichem Dateinamen getrennt bleiben. Die Stapelausgabe nutzt den ZIP-Modus; das Speichern in einen unkomprimierten Ordner bleibt für jeweils eine Linux-App verfügbar. Das Limit von 256 MiB für Downloads aus dem Arbeitsspeicher gilt für das gesamte Paket; nutze für größere Stapel daher **Save ZIP to disk**. Wird ein Stapel abgebrochen oder schlägt eine App fehl, bricht der Konverter das unvollständige Paket ab; ein Fehler beim Aufräumen wird gemeldet, statt das Paket als fertiges Ergebnis anzubieten.

Bei **Save ZIP to disk** steht Cancel zur Verfügung, bis der Browser beginnt, das fertige ZIP festzuschreiben.

### Konvertierungsbeleg im Browser

Jede exportierte App enthält `anyps5-conversion.json` neben der Programmdatei. Der Beleg hält das Release-`tag`, die vollständige Commit-`revision`, das `target` und die Angabe fest, ob die Option zur Intel-Übersetzung aktiviert war (`toIntel`). Ein Stapel-ZIP enthält in jedem nummerierten App-Ordner einen Beleg. Der Beleg enthält keine Pfade und keine Eingabedateinamen. Der Konverter zeigt nach Abschluss außerdem Release-Tag und Revision an. Hat das Release-Manifest keine gültige Revision, trägt der Beleg für `revision` den Wert `null` ein.

## Download eines Releases prüfen

Lade die Datei von den [offiziellen AnyPS5-Releases](https://github.com/boykopovar/AnyPS5/releases) herunter. Kopiere unter **Assets** des jeweiligen Release den `sha256:`-Digest neben dem genauen Dateinamen. Wurde die Datei erfolgreich heruntergeladen, berechne ihren SHA-256-Hash, bevor du sie öffnest:

- Windows PowerShell: `Get-FileHash -Algorithm SHA256 -LiteralPath "path\to\downloaded-file"`
- Linux: `sha256sum -- "path/to/downloaded-file"`

- macOS: `shasum -a 256 -- "path/to/downloaded-file"`

Vergleiche den Hash mit dem Teil nach `sha256:`, ohne Groß- und Kleinschreibung zu beachten. Weicht er ab, verwirf die Datei. Ein übereinstimmender Hash bestätigt, dass die Datei dem veröffentlichten Asset entspricht.

Blockiert ein Browser oder Virenschutz den Download, lass ihn blockiert. Melde den Release-Link, den Dateinamen, die genaue Warnung und den Browser oder Virenschutz, der sie angezeigt hat. Gib die Dateigröße in Bytes und den Hash an, falls die Datei verfügbar ist.

## Eingabe und Konvertierung

Der Relinker entpackt und entschlüsselt keine Paketdateien. Er benötigt eine entpackte, entschlüsselte ELF-Programmdatei und die mitgelieferten Module.

### Größenlimit für SELF-Dateien

Der SELF-Unwrapper lehnt einen Container ab, der insgesamt mehr als 64 MiB Ausrichtungs-Padding benötigt. Er prüft das, bevor er den Ausgabepuffer anlegt. Das Limit zählt das Padding über dateigestützte und leere Segmente hinweg, nicht die Segmentdaten. Lehnt er ab, bleiben deine Eingabe und eine vorhandene Ausgabedatei unverändert.

### Wohin die Dateien gehören

Verwende eine saubere ELF-Programmdatei. Lege die mitgelieferten ELF-Module in `sce_module/`, `sce_modules/` oder `prx/` neben die Eingabe-Programmdatei oder unter das mit `--sce-module-path <path>` gewählte übergeordnete Verzeichnis.

- `--sce-module-path` akzeptiert absolute Pfade und Pfade relativ zum aktuellen Arbeitsverzeichnis. Gib das übergeordnete Verzeichnis an, nicht das Verzeichnis `sce_module` selbst.
- Verarbeitet werden nur Dateien, die direkt in den Modulverzeichnissen liegen.
- SELF/fSELF-Container werden mit einer Warnung übersprungen. Entpacke sie vor dem Relinken, wenn sie einbezogen werden sollen.
  - `tools/unwrap_self.py <input.self> <output.elf>` erledigt das für einen entschlüsselten Container. Es legt die Segmente an die Offsets der SELF-Segmenteinträge und stellt `p_offset % p_align == p_vaddr % p_align` wieder her. Einen Container, dessen Load-Segmente alle verschlüsselt aussehen, lehnt es ab.
- Sicherungskopien werden mit einer Meldung übersprungen, sowohl auf der Kommandozeile als auch im Online-Konverter. Ein ELF-Modul gilt als Sicherung, wenn sein Dateiname den Namen eines anderen ELF-Moduls im selben Verzeichnis verlängert und beide denselben SONAME haben oder, falls keines einen SONAME hat, beide dieselben Modulnamen exportieren. Beispiel: `libX.prx.esbak` oder `libX.prxbak` eines Dumpers neben einem ELF `libX.prx`. Eine einzelne Sicherung neben einem SELF-Container wird trotzdem relinkt.
- `prx/` kann zusammen mit `sce_module/` oder `sce_modules/` vorhanden sein.
- Sind sowohl `sce_module/` als auch `sce_modules/` vorhanden, ist das ein Fehler.
- Fehlen alle drei, ist das ein Fehler, sofern du nicht explizit `--module-dir` angibst.

### Modulstart unter Linux, macOS und Windows

Importiert die Programmdatei `sceKernelLoadStartModule`, wird ein mitgeliefertes Modul, das weder die Programmdatei noch ein benötigtes Modul braucht, beim Programmstart nicht gestartet. Es initialisiert sich, wenn das Spiel es lädt, mit den Argumenten aus `sceKernelLoadStartModule`. Ohne diesen Import starten alle mitgelieferten Module beim Programmstart. Unter macOS wurde das Laden eines zurückgestellten Moduls auf diese Weise noch nicht ausgeführt.

### Aufbau und Befehl

```text
source/
    input.elf
    sce_module/
        <bundled ELF modules>
```

```text
relinker [options] <input.elf> <output>
```

### Probelauf

`relinker --dry-run [options] <input.elf> <output>` führt die Konvertierung im Arbeitsspeicher aus, einschließlich mitgelieferter Module, zusätzlicher Programmdateien, Importanalyse und Byte-Patches.

- Er listet jede geplante Ausgabe auf: Registry-Dateien, Dateien benötigter Bibliotheken, Patch-Manifeste, Windows-Diagnosen und Linux-Desktop-Metadaten.
- Er legt keine Verzeichnisse an, schreibt keine Dateien und ändert keine Berechtigungen. Vorhandene Ausgaben bleiben erhalten.
- Konvertierungsfehler liefern `2`, wie bei einem normalen Lauf.
- Der Ausgabepfad ist weiterhin erforderlich, weil er den Aufbau bestimmt.
- Er prüft weder den Schreibzugriff auf das Ziel noch Dateisystem-Aliase von Dateien, die noch nicht existieren. Diese Prüfungen führt nur eine normale Konvertierung durch.
- `--dry-run` ist nicht mit `--autorun` und `--verify-patches` kombinierbar.

### Modulordner ohne Standardnamen

Ein explizites `--module-dir` funktioniert auch, wenn das übergeordnete Modulverzeichnis kein Verzeichnis `sce_module`, `sce_modules` oder `prx` enthält. Für ein Homebrew-Paket mit gemeinsam genutzten Bibliotheken neben der Programmdatei verwende `--module-dir .`. Ein leeres Standardverzeichnis ist nicht nötig. Ohne explizites Modulverzeichnis bleibt die Anforderung an ein Standardverzeichnis bestehen.

### Benötigte Module, die nicht konvertiert wurden

Ein Modul, das die Programmdatei benötigt, das aber nicht konvertiert wird, wird im übergeordneten Modulverzeichnis außerhalb der Modulverzeichnisse gesucht. Eine Datei gilt als Treffer, wenn sie ein SELF-Container oder ein `ET_SCE_DYNAMIC`-ELF ist, dessen Name bis auf das Suffix `.prx`, `.sprx`, `.suprx` oder `.debug_prx` übereinstimmt. Mit `--windows` wird Groß- und Kleinschreibung ignoriert, wie beim Abgleich der Gastmodule.

- Jeder Treffer wird nach der Konvertierung gemeldet, zusammen mit dem `--module-dir`, das ihn einbeziehen würde.
- Die Ausgabe verweist weiterhin auf eine Host-Bibliothek dieses Namens.
- Eine Datei, die nicht gelesen werden kann, wird nicht gemeldet.

### Beispielbefehle

Linux-Ausgabe:

```sh
relinker source/input.elf app.elf
```

Windows-Ausgabe:

```sh
relinker --windows source/input.elf app.exe
```

macOS-Ausgabe (x86-64 Mach-O, läuft unter Rosetta auf Apple silicon):

```sh
relinker --macos source/input.elf eboot
```

Die Programmdatei und die Modulbibliotheken tragen eine Ad-hoc-Codesignatur, sodass sie ohne `codesign` laufen. Eine erneute Signatur mit `codesign -s - -f` oder deiner eigenen Identität ersetzt sie. Nach einer Bearbeitung mit `install_name_tool` oder `strip` signiere die Datei erneut.

Füge `--to-intel` für Intel-Hosts oder `--to-rosetta` für Rosetta hinzu. Ein Spiel, das unter Rosetta auf Apple silicon läuft, wird mit `--macos --to-rosetta` relinkt.

- `--to-intel` schreibt AMD-spezifische Instruktionen um, die in der Programmdatei und den mitgelieferten Modulen gefunden werden. AMD-spezifische Instruktionen, die ein Spiel zur Laufzeit erzeugt, kann es nicht übersetzen; solche Spiele brauchen einen AMD-Host. Ohne die Option beendet die erste solche Instruktion auf einer CPU, der sie fehlt, den Lauf, und unter Linux und Windows nennt der Absturzbericht die Instruktion.
- Das Ausgabeformat ist unabhängig vom Dateinamen standardmäßig Linux-ELF. `.exe` allein wählt nicht Windows.

### Sicherheitshinweis

Ein konvertiertes Spiel ist ein gewöhnliches Programm. Es läuft mit den Rechten deines Benutzerkontos und kann alles lesen und schreiben, was dein Konto kann. Die Sandbox der Konsole fehlt vollständig. Konvertiere und starte nur Spiele und Module, denen du vertraust. Führe sie als separaten Benutzer oder in einem Container oder einer virtuellen Maschine aus, um sie von deinen Dateien fernzuhalten.

Seine Programmdatei wird außerdem nicht per Address Space Layout Randomization platziert:

- **Linux:** wird bei `0x400000` oder an der Adresse von `--linux-fixed-base` geladen, sofern nicht das veraltete `--lazy-binding` sie positionsunabhängig hält.
- **Windows:** die niedrigste ELF-Load-Page wird an der bevorzugten Adresse `0x400000` geladen, wenn dieser Bereich frei ist, oder an der Adresse von `--windows-fixed-base`, ohne `DYNAMIC_BASE`.
- **macOS:** die Programmdatei wird ohne `MH_PIE` geschrieben.

### CPU-Anforderungen

Spiele laufen nativ, daher braucht die Host-CPU die Erweiterungen der Zen-2-CPU der PS5, die kompilierter Code ohne Prüfung verwendet. Dazu gehören AVX2, FMA, F16C, BMI1, BMI2, LZCNT und MOVBE.

Unter Linux und Windows protokolliert ein konvertiertes Spiel beim Start die Erweiterungen der Host-CPU. Fehlt eine, hält es an, bevor Spielcode läuft, und nennt die CPU und die fehlenden Erweiterungen ([Liste](../dev/debt/libkernel.md)).

### Systemanforderungen unter Linux

Die Linux-Systembibliotheken benötigen glibc 2.38 oder neuer und die C++-Bibliothek von GCC 13 oder neuer (`GLIBCXX_3.4.31`), wie in Ubuntu 24.04, Debian 13, Fedora 39 und deren späteren Releases. Auf einem älteren System wie Ubuntu 22.04, Debian 12 oder Fedora 38 lädt keine von ihnen, und das Spiel bricht vor dem Start mit einer Zeile wie dieser ab:

```text
./app.elf: /lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.38' not found (required by /path/to/libs/libc.prx)
```

Die `prx-linux`-Archive von v0.1.4 und v0.1.5 benötigen glibc 2.43, wie in Ubuntu 26.04 und Fedora 44.

Die Linux-Release-Datei `relinker-<version>` ist statisch gelinkt und läuft auch auf älteren Systemen. Ein Browser-Download hat keine Ausführungsberechtigung, führe daher einmal `chmod +x relinker-<version>` aus.

### Kollisionen von Ausgabedateien

Die Ausgabedateien für Programmdatei, Registry, Windows-Importdiagnose, Icon und Desktop-Eintrag dürfen nicht auf die Eingabe-Programmdatei oder ein zu konvertierendes mitgeliefertes Modul verweisen. Das schließt Hardlinks und symbolische Links ein.

- Solche Kollisionen schlagen fehl, bevor eine Ausgabe geschrieben wird.
- Eine vorhandene Ausgabedatei kann ersetzt werden, wenn sie von diesen Eingaben getrennt ist.
- Ein Ausgabepfad, der ein vorhandenes Verzeichnis ist, schlägt ebenfalls fehl, bevor eine Ausgabe geschrieben wird.

Unter Linux und macOS erfordert das Ersetzen einer regulären Ausgabedatei Schreibzugriff auf ihr Verzeichnis. Schlägt das Anlegen der temporären Datei oder das Ersetzen der alten Datei fehl, erhältst du einen Fehler. Die vorherige Datei wird nie an Ort und Stelle überschrieben.

## Optionen

Alle Schalter sind standardmäßig deaktiviert. `unused-filter` ist standardmäßig `0`; `--rpath` ist standardmäßig `$ORIGIN/libs`.

| Option                        | Wirkung |
|-------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `--help`                      | Gibt die Verwendung auf stdout aus und beendet sich mit Code `0`; Eingabe- und Ausgabepfad sind nicht erforderlich. |
| `--version`                  | Gibt die von `git describe` gemeldete Build-Revision des Relinkers auf stdout aus und beendet sich mit Code `0`; Eingabe- und Ausgabepfad sind nicht erforderlich. |
| `--`                          | Behandelt alle übrigen Argumente als Pfade, auch Namen, die mit `-` beginnen. |
| `--windows`                   | Erzeugt eine Windows-PE-Programmdatei. |
| `--macos`                     | Erzeugt eine x86-64-Mach-O-Programmdatei, mit den mitgelieferten Modulen als Mach-O-Bibliotheken unter `app0/`, jeweils in dem Verzeichnis, in dem sie gefunden wurden (`sce_module`, `sce_modules`, `prx` oder ein `--module-dir`). Nicht kombinierbar mit `--windows`. |
| `--windows-diagnostics`       | Schreibt Diagnosen zu Startabhängigkeiten nach `windows-diagnostics-imports.txt` neben die Ausgabe-Programmdatei. Erfordert `--windows`. |
| `--windows-gui`               | Wählt das Windows-GUI-Subsystem statt des Konsolen-Subsystems. Erfordert `--windows`. |
| `--windows-fixed-base <address>` | Bildet die niedrigste ELF-Load-Page an dieser festen Windows-Adresse statt an der bevorzugten `0x400000` ab, dezimal oder hexadezimal (`0x`) angegeben. Erfordert `--windows`, eine auf 64 KiB ausgerichtete Adresse von `0x20000` bis `0x7fffffff0000` und ein vollständiges Image, das in den Benutzeradressraum passt. Die Hauptprogrammdatei hat weder ASLR noch Basisrelokationen; eine belegte Adresse führt zu einem Ladefehler. Mitgelieferte Module behalten ihre normale Platzierung. |
| `--trace-imports`             | Leitet jeden Funktionsimport durch den Aufruf-Tracer von libc.prx, siehe [Aufrufe von Systembibliotheken verfolgen](#aufrufe-von-systembibliotheken-verfolgen). Ausgabe für Windows, Linux und macOS; nicht kombinierbar mit `--lazy-binding`. |
| `--linux-fixed-base <address>` | Lädt die Linux-Programmdatei an dieser festen Adresse statt an der Standardadresse `0x400000`, dezimal oder hexadezimal (`0x`) angegeben: Jede virtuelle Adresse des Images wird um sie verschoben, sodass die virtuelle Adresse 0 an dieser Adresse liegt, so wie die Konsole eine Programmdatei platziert. Erfordert eine auf 64 KiB ausgerichtete Adresse von `0x10000` bis `0x7fffffff0000`, die ein Vielfaches jeder `PT_LOAD`-Ausrichtung ist, und ein Image, das im Benutzeradressraum endet. Die Programmdatei wird als `ET_EXEC` ohne ASLR geschrieben, wie an der Standardadresse. Mitgelieferte Module behalten ihre normale Platzierung. Nicht kombinierbar mit `--windows`, `--macos` und `--lazy-binding`. |
| `--macos-fixed-base <address>` | Bildet die niedrigste ELF-Load-Page der macOS-Programmdatei an dieser festen Adresse ab, dezimal oder hexadezimal (`0x`) angegeben. Erfordert `--macos`, eine auf 64 KiB ausgerichtete Adresse von `0x20000` bis `0x7fffffff0000` und ein Image, das im macOS-Benutzeradressraum endet. `__PAGEZERO` endet 64 KiB unterhalb der Adresse, und der Mach-O-Header belegt diese 64 KiB. Die Programmdatei hat kein ASLR; eine Adresse, die der Prozess nicht abbilden kann, führt zu einem Ladefehler. Mitgelieferte Module behalten ihre normale Platzierung. |
| `--to-intel`                  | Konvertiert unterstützte reine AMD-Instruktionen in der Programmdatei und den mitgelieferten Modulen. Nicht unterstützte Instruktionen oder nicht erreichbare Konvertierungs-Stubs führen zu einem Fehler. |
| `--to-rosetta`                | Konvertiert wie `--to-intel` und ersetzt zusätzlich, was Rosetta fehlt: ANDN wird zu skalarem NOT/AND, BLSI, BLSMSK und BLSR werden zu skalaren LEA/AND/XOR/NEG-Sequenzen, SHLX, SHRX, SARX und RORX werden zu flagerhaltenden skalaren Shifts und Rotationen, BZHI und BEXTR werden zu skalaren Shift-and-Mask-Sequenzen, RDSEED wird zu RDRAND, CLWB zu einem NOP, XSAVEOPT wird zu XSAVE, und RDPID sowie das ECX von RDTSCP liefern Prozessor 0. Impliziert `--to-intel`. Unabhängig von `--macos`: `--macos` allein lässt diese Instruktionen unverändert. Nicht kombinierbar mit `--windows`. |
| `--to-avx`                    | Konvertiert wie `--to-intel` und ersetzt zusätzlich, was einer CPU mit AVX, aber ohne AVX2 fehlt: ANDN, BLSI, BLSMSK, BLSR, SHLX, SHRX, SARX, RORX, BZHI, BEXTR, MULX, CLWB und RDPID wie bei `--to-rosetta`, TZCNT und LZCNT werden zu BSF und BSR mit einem CMOV für eine Quelle mit dem Wert null, und MOVBE wird zu MOV und BSWAP. VINSERTI128, VEXTRACTI128, VPERM2I128, VPBLENDD, VBROADCASTI128 und die Speicherformen von VPBROADCASTD und VPBROADCASTQ werden an Ort und Stelle zu ihren AVX-Entsprechungen VINSERTF128, VEXTRACTF128, VPERM2F128, VBLENDPS, VBROADCASTF128, VBROADCASTSS und VBROADCASTSD bzw. VMOVDDUP, 256-Bit-Integer-Instruktionen, die innerhalb einer Lane arbeiten (Additionen, Logik, Vergleiche, Packs, Unpacks, Shuffles innerhalb einer Lane, Shifts um einen Immediate-Wert oder eine XMM-Anzahl, VPBLENDVB, VMPSADBW und VPMOVMSKB), laufen als zwei 128-Bit-Hälften in einem Stub, und die Lane-übergreifenden Permutationen VPERMQ, VPERMPD, VPERMD und VPERMPS, die 256-Bit-Erweiterungsmoves VPMOVZX und VPMOVSX sowie die Broadcasts mit Registerquelle und die Byte- und Word-Broadcasts laufen in AVX-Stubs. Die Rosetta-Workarounds für 256-Bit-Stores, skalare Vergleiche, maskierte Stores, RDTSCP, XSAVEOPT und RDSEED werden nicht angewendet. Andere AVX2-Instruktionen, F16C, PDEP und PEXT werden noch nicht ersetzt ([Liste](../dev/debt/relinker.md)). Impliziert `--to-intel`. Erfordert `--macos`, weil nur die macOS-Laufzeitumgebung startet, ohne die Host-CPU zu prüfen. |
| `unused-filter=0`             | Behält alle importierten NID-Referenzen. |
| `unused-filter=1`             | Filtert ungenutzte Nicht-PLT-Importe per Kontrollfluss- und GOT-Zugriffsanalyse, wobei Code, dessen Adresse genommen wird, als erreichbar gilt; PLT-Importe bleiben erhalten. |
| `unused-filter=2`             | Wendet eine strikte Analyse ungenutzter Importe an und verdichtet die PLT. Nicht unterstützte Analysefälle führen zu einem Fehler. |
| `--registry`                  | Schreibt `<output-stem>.registry.json` neben die Ausgabe-Programmdatei und `<output-stem>.<module>.registry.json` für jedes konvertierte mitgelieferte Modul. |
| `--rpath <path>`              | Setzt den Suchpfad für Systembibliotheken. Setze `$ORIGIN` in Anführungszeichen, um die Shell-Expansion zu verhindern, zum Beispiel `--rpath '$ORIGIN/libs'` in Bash oder PowerShell. Unter Linux und macOS darf kein durch Doppelpunkte getrennter Eintrag leer sein, und mit Gastmodulen muss jeder absolut sein oder mit `$ORIGIN` beginnen. Windows erfordert einen nicht leeren ASCII-Pfad und unterstützt `$ORIGIN` als Verzeichnis der Programmdatei. |
| `--omit-unavailable-dependencies` | Linux-Ausgabe: Lässt eine Abhängigkeit eines mitgelieferten Moduls weg, die kein konvertiertes Modul bereitstellt, aus der das Modul nichts importiert und die beim Konvertieren in keinem `--rpath`-Verzeichnis liegt. Jedes Run-Path-Verzeichnis muss zu diesem Zeitpunkt existieren. Ohne den Schalter bleibt eine solche Abhängigkeit erhalten, und die Konvertierung gibt einen Hinweis mit ihrem Namen aus. Verwende ihn nur, wenn der Run-Path jede Systembibliothek enthält: Eine Bibliothek, die erst später über `LD_LIBRARY_PATH` gefunden wird, wird nicht mehr geladen. Nicht mit `--windows` oder `--macos` verwendbar. |
| `--autorun`                   | Führt die Ausgabe nach der Konvertierung aus, gibt ihren Exit-Code aus und wartet auf Enter. Setze `ANYPS5_AUTORUN_NO_PAUSE=1`, um das Warten zu überspringen. Erfordert das Zielbetriebssystem und den vorbereiteten Aufbau für die Laufzeitumgebung. |
| `--dry-run`                   | Konvertiert im Arbeitsspeicher, einschließlich mitgelieferter Module, zusätzlicher Programmdateien, Importanalyse und Byte-Patches, und listet die geplanten Ausgaben auf, ohne Verzeichnisse anzulegen, Dateien zu schreiben oder Berechtigungen zu ändern. Konvertierungsfehler liefern `2`; der Ausgabepfad ist weiterhin erforderlich. Nicht kombinierbar mit `--autorun` und `--verify-patches`. |
| `--sce-module-path <path>`    | Übergeordnetes Verzeichnis der Modulverzeichnisse, nicht das Modulverzeichnis selbst. Standard: übergeordnetes Verzeichnis der Eingabe-ELF; relative Eingabepfade werden vom Arbeitsverzeichnis des Prozesses aus aufgelöst. Expliziter absoluter Pfad: wird direkt verwendet. Expliziter relativer Pfad (einschließlich `.` und `..`): relativ zum Arbeitsverzeichnis des Prozesses. Der Ausgabepfad hat keinen Einfluss. Erfordert einen nicht leeren Wert; Wiederholungen, nicht vorhandene Pfade oder Pfade, die keine Verzeichnisse sind, Zugriffsfehler und die Kombination mit `--skip-sce-module` sind Fehler. |
| `--module-dir <dir>`          | Relinkt die ELF-Module eines Verzeichnisses unter dem übergeordneten Modulverzeichnis, zusätzlich zu `sce_module`/`sce_modules`/`prx`; die Ausgaben behalten den relativen Pfad. Benötigte Module werden beim Start abgebildet. Unter Linux initialisieren sich Module in diesen zusätzlichen Verzeichnissen nur, wenn sie explizit geladen werden, mit den Argumenten aus `sceKernelLoadStartModule` (zum Beispiel Unity-Plugins in `Media/Plugins`). Unter Windows initialisiert sich ein solches Modul, wenn es explizit geladen wird, nach den von ihm benötigten Modulen, die sich auf dieselbe Weise initialisieren (sie erhalten dieselben Startargumente), es sei denn, ein Modul außerhalb dieser Verzeichnisse benötigt es direkt oder über andere Module, dann initialisiert es sich beim Start; das erste explizite Laden eines solchen Moduls initialisiert außerdem nach Möglichkeit die übrigen, von denen ein Modul abhängt. Unter macOS initialisiert sich ein Modul, das ein anderes Modul oder die Programmdatei benötigt, beim Start. Andere Module initialisieren sich beim Laden. Wiederholbar; `.` ist das übergeordnete Verzeichnis selbst, ohne die Eingabe-ELF. Nicht kombinierbar mit `--skip-sce-module`. |
| `--executable <path>`         | Konvertiert zusätzlich eine weitere Programmdatei des Spiels, die es mit `sceSystemServiceLoadExec` startet; der Pfad ist relativ zum übergeordneten Modulverzeichnis, zum Beispiel `picker/picker.bin`. Sie wird neben die Ausgabe geschrieben, nach dem Pfad benannt, wobei `/` durch `.` ersetzt wird, erhält die Erweiterung der Ausgabe (`picker.picker.bin.exe`) und verwendet dieselben `libs/`, `app0/` und Gastmodule. Es ist ein Fehler, wenn ein Gastmodul für sie anders konvertiert wird. Wiederholbar. Nicht kombinierbar mit `--skip-sce-module`. |
| `--patches <file>`            | Wendet die Byte-Patches dieser Datei während der Konvertierung an, siehe [Spiel-Patches](#spiel-patches). Hat Vorrang vor dem automatischen Auffinden von `relinker-patches.toml`. Nicht kombinierbar mit `--no-patches`. |
| `--no-patches`                | Wendet keine Patches an und lädt `relinker-patches.toml` nicht aus dem Eingabeverzeichnis. |
| `--verify-patches <file>`     | Prüft eine Patch-Datei gegen `<path>`, ohne etwas zu schreiben, siehe [Spiel-Patches](#spiel-patches). Nimmt einen Pfad und keinen Ausgabepfad. |
| `--verify-output`             | Behandelt `<path>` zusammen mit `--verify-patches` als konvertierte Linux- oder Windows-Ausgabe statt als Eingabe-Programmdatei. |
| `--function-symbol-roots`     | Behandelt definierte Funktionssymbole als Code-Einstiegspunkte, statt bei der Exportvalidierung und der Codeanalyse für Windows/macOS ihren `st_size`-Bereichen zu vertrauen. Für Module, deren Symbolgrößen anderen Code beschreiben, etwa generierte Loader-Begleitmodule. Funktionseinträge müssen ausführbar und dateigestützt bleiben; Daten-, TLS- und Unwind-Bereiche behalten ihre Prüfungen. Die ursprünglichen Symbolgrößen bleiben erhalten. |
| `--skip-sce-module`           | Veraltet. Deaktiviert die Verarbeitung mitgelieferter Module und die Prüfungen der Modulverzeichnisse. Nicht kombinierbar mit einem expliziten `--sce-module-path`. |
| `--exclude-sce-module <file>` | Veraltet. Schließt ein mitgeliefertes Modul anhand des exakten Dateinamens aus, nicht anhand des Pfads. Für mehrere Dateien wiederholen; ein fehlender Dateiname ist ein Fehler. Nicht kombinierbar mit `--skip-sce-module`. |
| `--skip-syscall-check`        | Veraltet. Deaktiviert die Syscall-Suche in der Programmdatei und den mitgelieferten Modulen. |
| `--trap-sdk-syscall-stubs`    | Veraltet. Nur zum Debuggen. Ersetzt die exakten generischen und nummerierten Syscall-Wrapper-Stubs von ps5-payload-dev und die erkannten Inline-mmap/mprotect-Sequenzen aus SDK libc/mman.c sowie die inline eingefügten `__syscall6`-Stubs (`mount`, `nmount`, `unmount`) aus SDK libc/mount.c durch `UD2`, damit ungenutzte Wrapper das Relinken nicht blockieren. Andere Syscall-Instruktionen führen weiterhin zu einem Fehler, außer einem `int 0x80; ret` zwischen den Einträgen `int 0x7f; ret` und `int 0x81; ret` einer Software-Interrupt-Tabelle, das auch ohne diese Option durch `UD2` ersetzt wird. Läuft ein abgefangener Wrapper, löst er eine Illegal-Instruction-Exception aus. (Hinweis: Der nummerierte ps5-payload-dev-Stub 191, `pathconf`, wird beim Import automatisch auf den `pathconf`-Export von `libkernel` umgeleitet und benötigt diese Option nicht.) |
| `--frame-red-zone`            | Nur Windows: Gibt Leaf-Funktionen, die Werte unterhalb von RSP halten (die System-V-Red-Zone), einen Frame, damit eine Exception, die Windows auf einem Gast-Thread zustellt, sie nicht überschreiben kann. Erfasst Leaf-Funktionen mit einem Prolog `push rbp; mov rbp, rsp; push ...`, Leaf-Funktionen, die nur Register pushen, mit oder ohne `sub`-Allokation nach den Pushes, und Leaf-Funktionen ohne beides; ein Prolog oder Ausgang, der kürzer als ein Sprung ist, bezieht die benachbarten Instruktionen ein. In Leaf-Funktionen ohne Frame-Pointer wird RSP nur so weit abgesenkt, wie die Funktion unter RSP reicht, und ihre RSP-Operanden werden mitverschoben, an Ort und Stelle oder, wenn ihre Kodierung das verschobene Displacement nicht aufnehmen kann, in einem Stub. Funktionsgrenzen stammen aus dem `.eh_frame` der Programmdatei; fehlt es, bricht der Relinker mit einem Fehler ab. Der Relinker gibt aus, wie viele er mit einem Frame versehen hat und warum er die anderen übersprungen hat, und lehnt einen Spiel-Patch über einer so bearbeiteten Stelle ab. |
| `--lazy-binding`              | Veraltet. Aktiviert verzögertes Symbol-Binding statt sofortigem Binding. Nicht kompatibel mit mitgelieferten ELF-Modulen. Die Linux-Programmdatei bleibt positionsunabhängig (`ET_DYN`), statt bei `0x400000` platziert zu werden. Nur Ausgabe für Windows und Linux. |

Gib `unused-filter=0|1|2` ohne `--` an, höchstens einmal. Unbekannte Optionen und zusätzliche Positionsargumente sind Fehler. `relinker --help` gibt die Syntax auf stdout aus und beendet sich mit Code `0`, ohne Dateien zu konvertieren. Ein Aufruf von `relinker` ohne Argumente gibt die Syntax auf stderr aus und beendet sich mit Code `1`.

Die Optionen `--skip-sce-module`, `--exclude-sce-module <file>`, `--skip-syscall-check`, `--lazy-binding` und `--trap-sdk-syscall-stubs` sind veraltet. Läuft die Anwendung mit diesen aktivierten Optionen, ist sie extrem instabil und für den allgemeinen Gebrauch ungeeignet. Diese Optionen dienen nur dem Debuggen.

`--trap-sdk-syscall-stubs` dient ebenfalls dem Debuggen. Es erhält einen Laufzeitfehler, wenn ein Syscall-Wrapper aufgerufen wird; es implementiert keine FreeBSD-Syscalls und macht diese Anwendungen nicht allgemein sicher ausführbar. Der nummerierte Syscall 191 (`pathconf`) aus ps5-payload-dev wird beim Relinken auf den `pathconf`-Export von `libkernel` umgeleitet, wenn er importiert wird.

## Spiel-Patches

Eine Patch-Datei ändert vor der Konvertierung Bytes der Eingabe-Programmdatei oder eines mitgelieferten Moduls, für ein Spiel, das einen Wert fest einprogrammiert, den der Host nicht erfüllen kann. Der Relinker prüft die Bytes, die er ersetzt, sodass ein Patch für eine andere Version des Spiels fehlschlägt, statt sie zu beschädigen. Ohne Patch-Datei bleibt die Ausgabe unverändert.

Advisor und Katalog-Validator erfordern Python 3.11 oder neuer. Um einen lokalen Patch-Katalog zu prüfen, führe aus:

```sh
python3 tools/suggest_patches.py --catalog <directory> --param-json <sce_sys/param.json> --input <input.elf> --relinker <relinker> [--sce-module-path <directory>] [--module-dir <directory>]... [--executable <path>]... [--skip-sce-module] [--timeout-seconds <seconds>] [--list-verified] [--json]
```

Um vor dem Teilen jeden Eintrag eines Katalogs ohne Spieldateien und ohne Relinker zu prüfen, führe aus:

```sh
python3 tools/validate_patch_catalog.py --catalog <directory>
```

Der Validator prüft das Indexformat, doppelte JSON-Keys, Pflicht- und optionale Felder, relative Patch- und Zielpfade, das Vorhandensein der Patch-Dateien, die TOML-Struktur, die Abdeckung der Ziel-Hashes und doppelte `id`-Einträge, die auf denselben Spiel-Build passen könnten. Einträge mit derselben `id` und derselben Inhaltsversion dürfen nebeneinander bestehen, wenn ein gemeinsames Ziel unterschiedliche Hashes hat, sodass sich ihre Treffer gegenseitig ausschließen. Um eine neue `revision` für denselben Build zu veröffentlichen, entferne den alten Eintrag. Ein leeres `entries`-Array ist gültig und meldet `0 entries valid`.

Der Validator meldet jeden ungültigen Eintrag und endet mit Exit-Code `2`, wenn ein Eintrag ungültig ist; ein gültiger Katalog endet mit `0`. Das ist eine Strukturprüfung: Der Relinker akzeptiert eine strengere Teilmenge von TOML, und nur `relinker --verify-patches` mit den Spieldateien kann feststellen, ob sich der Patch parsen lässt und zu diesen Dateien passt. Der Validator liest keine Zieldateien, lädt nichts herunter und wendet keine Patches an.

Der Advisor liest `titleId` und `contentVersion` aus der angegebenen `param.json`, prüft den SHA-256 jeder Datei, auf die ein Kandidaten-Patch zielt, und ruft dann den mit `--relinker` angegebenen Relinker auf, um `--verify-patches` auszuführen. Ein relativer Pfad wie `--relinker ./relinker` wird zu dieser lokalen Datei aufgelöst. Der explizite Metadatenpfad funktioniert auch, wenn `param.json` und die entpackten ELF-Dateien getrennt gespeichert sind. Das Katalogverzeichnis muss eine `index.json` mit `format: 1` und einem `entries`-Array enthalten. Jeder Eintrag hat `titleId`, `contentVersion`, einen relativen `patch`-Pfad und `targets`, das jedes Patch-Ziel seinem SHA-256 zuordnet. Bei hexadezimalen Digests spielt Groß- und Kleinschreibung keine Rolle. Zum Beispiel:

```json
{
  "format": 1,
  "entries": [{
    "titleId": "TEST00001",
    "contentVersion": "01.000.000",
    "patch": "patch.toml",
    "targets": {"Media/Modules/module.prx": "<SHA-256 in hexadecimal>"}
  }]
}
```

Katalogeinträge können außerdem diese optionalen Felder enthalten. Kataloge ohne sie verhalten sich unverändert und behalten `format: 1`. Ältere Advisor-Versionen lehnen passende Einträge mit diesen Feldern ab; verwende eine Advisor-Version, die sie unterstützt.

| Feld | Wert |
|-------|-------|
| `id` | Stabile Patch-Kennung, die `[a-z0-9][a-z0-9_-]*` entspricht. Sie darf für eine andere Inhaltsversion wiederverwendet werden. Verifizierte Einträge mit derselben ID für ein Spiel und eine Version werden abgelehnt. |
| `name` | Für Benutzer sichtbarer Name der gesamten Patch-Datei. |
| `category` | Eines von `compatibility`, `fps`, `graphics`, `memory` oder `other`. |
| `description` | Was die Patch-Datei ändert. |
| `tested` | Nicht leeres Array von Testeinträgen, siehe unten. |
| `notes` | Einschränkungen oder Anweisungen für den Benutzer. |
| `contributors` | Nicht leeres Array unterschiedlicher Namen von Beitragenden. |
| `source` | HTTPS-Link zur Herkunft des Patches. Der Advisor zeigt ihn nur an; er ruft ihn nicht ab. |
| `revision` | Positive ganzzahlige Revision der Patch-Definition, unabhängig von der `contentVersion` des Spiels. |

`category` beschreibt den Bereich des Patches, nicht ob seine Wirkung ein Fix oder eine Verbesserung ist; erkläre die Wirkung in `description`. `revision` wählt nicht automatisch den neuesten Eintrag. Um eine Patch-Definition mit derselben `id` für ein Spiel und eine Inhaltsversion zu ersetzen, entferne den alten Eintrag aus dem Katalog. Werden beide Revisionen verifiziert, lehnt der Advisor beide als doppelte IDs ab.

`name`, `description` und `notes` sind, sofern vorhanden, nicht leerer einzeiliger Text. Sie beschreiben den Katalogeintrag als Ganzes; `name` und `description` in `[[patch]]` einer Patch-TOML beschreiben einzelne Byte-Änderungen. Jeder `tested`-Eintrag erfordert `stage` (`verification`, `conversion`, `launch` oder `gameplay`), `result` (`passed`, `failed` oder `partial`) und eine nicht leere einzeilige `evidence`. Das optionale `environment` ist einzeiliger Text, das optionale `date` ein Kalenderdatum im Format `YYYY-MM-DD`. Zum Beispiel:

```json
{
  "tested": [{
    "stage": "verification",
    "result": "passed",
    "environment": "CachyOS Linux",
    "date": "2026-10-26",
    "evidence": "Relinker reported 1 ok, 0 not-checked, 0 mismatch"
  }]
}
```

Der Advisor validiert diese Felder für passende Katalogeinträge und meldet einen fehlerhaften Eintrag, ohne die Prüfung der übrigen Einträge abzubrechen. Metadaten gibt er nur für Einträge aus, die die Hash- und Relinker-Prüfungen bestehen. Testeinträge sind Angaben der Beitragenden; der Advisor bestätigt Nachweise zu Start oder Gameplay nicht selbst.

Der Advisor validiert Patch-Dateien nur für Einträge, die zum angefragten Spiel und zur angefragten Version passen. Einen fehlerhaften passenden Eintrag oder einen Verifizierungsfehler des Relinkers meldet er und überspringt ihn, sodass ein anderer gültiger Eintrag trotzdem empfohlen werden kann. Die Verifizierung bricht standardmäßig nach 120 Sekunden pro Eintrag ab; mit `--timeout-seconds <positive integer>` änderst du das Limit. Für synthetische Eingaben ohne Systemmodule verwende `--skip-sce-module`, wenn auch die Konvertierung es verwendet; diese Option ist nicht mit `--sce-module-path` kombinierbar. Kein passender Eintrag oder kein verifizierter Patch ergibt Exit-Code `1`; ungültige Eingaben oder kein verifizierter Patch nach einem Relinker-Fehler ergeben Exit-Code `2`. Werden mehrere Einträge verifiziert, empfiehlt der Advisor keinen, weil er sie weder auswählen noch kombinieren kann, und verweist auf `--list-verified`, um die Treffer zu prüfen. Das Tool ruft keine Patches ab und wendet keine an. Eine Empfehlung gibt `--patches` sowie die Modul- und Programmdatei-Optionen aus, die beim Konvertieren zu verwenden sind. Bewahre die Patch-Dateien des Katalogs außerhalb des Verzeichnisses der Eingabe-Programmdatei auf, damit `relinker-patches.toml` nicht automatisch gefunden wird.

Mit `--list-verified` gibt der Advisor in Katalogreihenfolge jeden Eintrag aus, der die Metadaten-, Hash- und Relinker-Prüfungen besteht. Jedes Ergebnis enthält seine Katalognummer, die Patch-Datei, die Ziel-Hashes und die Konvertierungsoptionen. Er endet mit `0`, wenn mindestens ein Eintrag verifiziert wird, auch wenn ein anderer Eintrag abgelehnt wurde; ohne verifizierte Einträge gilt weiterhin der bisherige Exit-Status `1` oder `2`. Ohne diese Option wird ein einzelner verifizierter Eintrag empfohlen, und mehrere verifizierte Einträge bleiben mehrdeutig. Die Auflistung weist Benutzer an, eine Patch-Datei auszuwählen. Sie kombiniert keine Patch-Dateien und wendet sie beim Konvertieren nicht an; prüfe den gewählten Patch, bevor du ihn anwendest.

Mit `--json` erhältst du ein einzelnes maschinenlesbares Ergebnis auf stdout. `schemaVersion` ist für jedes Ergebnis `1`, auch für Fehler; Konsumenten sollten es prüfen, bevor sie andere Felder lesen. Die bestehenden Felder `status`, `entries`, `recommended_entry` und die Exit-Codes behalten ihre Bedeutung. Jeder Eintrag mit `status: "verified"` hat außerdem `conversion_options`: ein Array einzelner Argumente wie `["--patches", "/absolute/catalog/patch.toml", "--skip-sce-module"]`. Das sind dieselben Optionen, die der Textmodus ausgibt, in derselben Reihenfolge; sie enthalten weder das Relinker-Programm noch die Eingabe-ELF. Relative Modulpfade behalten die Bedeutung, die sie beim Aufruf des Advisors hatten. Abgelehnte Einträge haben keine Konvertierungsoptionen. Das Array beschreibt eine verifizierte Patch-Datei, keine Kombination von Einträgen, und wendet keinen Patch automatisch an. Fehler beim Parsen der Argumente verwenden weiterhin die stderr-Ausgabe und den Exit-Status von argparse.

Die Ziel-Hashes identifizieren genau die entpackten Dateien, die ein Patch ändert; die Hauptprogrammdatei muss nicht darunter sein. Verschiedene Dump-Tools können für dieselbe Spielversion unterschiedliche Bytes erzeugen; der Advisor lehnt einen Patch dann ab, auch wenn er funktionieren würde. Es ist nicht bestätigt, dass alle Dump-Tools identische Dateien erzeugen. Die Hashes authentifizieren keinen Katalog und belegen weder, dass ein Patch sicher ist, noch, dass das Spiel vollständig spielbar ist. Verwende Kataloge aus Quellen, denen du vertraust, und prüfe den Patch, bevor du ihn anwendest.

Zielt der Patch auf eine andere Programmdatei des Spiels, übergib dem Advisor für jede zusätzliche Programmdatei `--executable <path>`, mit Pfaden relativ zum übergeordneten Modulverzeichnis, etwa `picker/picker.bin`. Der Advisor hasht diese Zieldateien und reicht dieselben Optionen an `relinker --verify-patches` weiter. Verwende beim Konvertieren mit `--patches` dieselben `--executable`-Optionen.

`relinker-patches.toml` im Verzeichnis der Eingabe-Programmdatei wird automatisch geladen und als `Patches: <path>` angekündigt. `--patches <file>` benennt eine andere Datei, `--no-patches` lädt keine. Jeder angewendete Patch wird gemeldet:

```
Patch: <target> [<name>] va=0x<address> off=0x<offset> <old> -> <new> (<description>)
```

Die Datei ist eine Teilmenge von TOML; jede andere TOML-Syntax ist ein Fehler, der die Zeile nennt. Sie ist in UTF-8 kodiert und enthält Leerzeilen, `#`-Kommentare außerhalb von Strings, `[[patch]]`-Einträge und Zeilen der Form `key = value`. Ganzzahlen sind dezimal oder hexadezimal mit `0x`, Strings stehen ohne Escapes in doppelten Anführungszeichen, und Keys entsprechen `[A-Za-z0-9_]+`.

```toml
format = 1

[[patch]]
name        = "pool_size"
target      = "eboot.elf"
va          = 0x1234
type        = "i32"
expected    = 0x10000000
value       = 0x18000000
max         = 0x7f000000
align       = 0x1000
description = "pool size"
```

`format = 1` ist erforderlich. Die Top-Level-Keys `relinker_version`, `relinker_commit`, `config`, `config_sha256`, `main_target`, `linux_base`, `settings` und `settings_sha256` werden in jeder Datei akzeptiert; verwendet werden nur `main_target` und `linux_base`, und zwar von `--verify-output`. Jeder andere Top-Level-Key ist ein Fehler.

| Eintrags-Key | Bedeutung |
|-----------|---------|
| `name` | Optional. Ein Einstellungsname aus `[a-z0-9_]`, eindeutig innerhalb der Datei und keiner der reservierten Top-Level-Key-Namen (`format` und die acht Keys oben). Wird in der Fortschrittszeile, im Manifest und im Prüfbericht angezeigt. |
| `target` | Erforderlich. Der Dateiname der Hauptprogrammdatei oder, bei `--executable`-Dateien und mitgelieferten Modulen, der Pfad relativ zum übergeordneten Modulverzeichnis, zum Beispiel `sce_module/libc.prx`. |
| `va` | Erforderlich. Die virtuelle Adresse der ELF, wie ein Disassembler sie anzeigt. |
| `output_rva` | Optional, von `0` bis `0xffffffff`. Wird von einer `--windows`-Konvertierung ins Manifest geschrieben: die Adresse des ersten Bytes des Eintrags in der PE-Ausgabe, relativ zu ihrer Image-Basis. Wird nur von `--verify-output` verwendet; eine Konvertierung ignoriert den Wert und schreibt einen eigenen. |
| `type` | Optional: `u8 u16 u32 u64 i8 i16 i32 i64`. Dann sind `expected`, `value`, `min`, `max` und `align` Ganzzahlen, werden auf ihren Wertebereich geprüft und Little-Endian geschrieben. Ohne diesen Key sind `expected` und `value` hexadezimale Byte-Strings gleicher Länge. |
| `expected` | Erforderlich. Die Bytes, die die Eingabe enthalten muss. |
| `value` | Erforderlich. Die zu schreibenden Bytes. |
| `min`, `max`, `align` | Optional, nur bei typisierten Einträgen; werden gegen `value` geprüft. |
| `description` | Optional. Wird gemeldet und ins Manifest übernommen. |

x86 `mov r/m64, imm32` und `add`/`sub`/`cmp r/m64, imm32` erweitern ihren Immediate-Wert vorzeichenrichtig. Um ein 64-Bit-Feld über ein imm32 zu patchen, verwende `type = "i32"` und füge `min` hinzu, um negative Werte auszuschließen.

Einträge werden in Dateireihenfolge angewendet. Ein Patch-Bereich muss in den Dateibytes eines einzigen `PT_LOAD` liegen und außerhalb des ELF-Headers und der Program-Header-Tabelle. Die Konvertierung schlägt fehl, bevor eine Ausgabe geschrieben wird, wenn Bytes von `expected` abweichen, ein Bereich ungültig ist, sich zwei Einträge überschneiden, ein Ziel auf keine oder mehr als eine Datei passt oder ein Bereich Bytes überschneidet, die der Relinker selbst umschreibt (Ersetzungen und Trampoline von `--to-intel`, Relinker-Patches, Syscall-Traps), oder das 8-Byte-Ziel einer Relokation der Datei.

Ein PLT-Stub der Programmdatei oder eines mitgelieferten Moduls, der nicht mehr mit dem gelinkten `jmp` über seinen GOT-Eintrag beginnt, wird während der Konvertierung gemeldet, weil der Import dieses Eintrags nicht über ihn aufgerufen wird:

```
Replaced PLT stub: [<target>] va=0x<address> <linked> -> <found> [(jump to 0x<address>)]; import <NID> is not called through it; a --patches entry with expected = "<found>" and value = "<linked>" restores the linked stub
```

`<target>` ist das Patch-Ziel des Moduls und entfällt bei der Programmdatei. Die Bytes sind der 6-Byte-Sprung oder der ganze 16-Byte-Stub, wenn sich auch sein `push` und sein `jmp` zum PLT-Header unterscheiden. Ansonsten bleibt die Konvertierung unverändert und konvertiert die Ersetzung so, wie sie ist. Eine Tabelle, in der kein Stub auf diese Weise gelinkt ist, wird nicht gemeldet. Wird in dem Code, zu dem ein solcher Stub springt, vor dem ersten Return oder Sprung dieses Codes eine Syscall-Instruktion abgelehnt, nennt der Fehler den Stub. Wird der Stub wiederhergestellt, ist dieser Code nicht mehr referenziert, wird aber weiterhin durchsucht; daher muss ein zweiter Eintrag ihn überschreiben, bevor sich die Datei konvertieren lässt.

Wenn mindestens ein Patch angewendet wird, wird `<output-stem>.patches.toml` neben die Ausgabe geschrieben. Die Datei listet jeden angewendeten Eintrag für jede konvertierte Datei auf, mit `main_target`, `linux_base` (die Adresse, an der eine Linux-`ET_EXEC`-Ausgabe platziert wird) und `config` (der Pfad der Patch-Datei mit Schrägstrichen, verkürzt auf den Dateinamen, wenn der Pfad ein `"`, einen Backslash oder ein Steuerzeichen enthält; die Konvertierung schlägt fehl, bevor eine Ausgabe geschrieben wird, wenn selbst dieser nicht aufgenommen werden kann); typisierte Einträge behalten ihren `type`, Ganzzahlen sowie `min`, `max` und `align`, und `--patches` akzeptiert die Datei, um die Konvertierung zu reproduzieren. Das Manifest darf nicht die Patch-Datei ersetzen, aus der es gelesen wurde.

`--verify-patches <file> <path>` schreibt nichts und endet mit `0`, wenn kein Eintrag abweicht. Ohne `--verify-output` ist `<path>` die Eingabe-Programmdatei beliebigen Typs, Module werden wie bei einer Konvertierung gefunden (Moduloptionen und `--executable` gelten), und jeder Eintrag ist `applicable` (die Eingabe enthält `expected`) oder `mismatch`. Ein Ziel, das auf nichts passt, ist ein Fehler, der `--verify-output` vorschlägt.

Mit `--verify-output` ist `<path>` eine konvertierte Linux-ELF oder Windows-PE; Mach-O-Ausgaben werden abgelehnt. Die Datei muss `main_target` enthalten; prüfe daher das erzeugte Manifest, keine von Hand geschriebene Datei. Einträge werden in einer `ET_EXEC`-Ausgabe an ihrer `va` plus dem `linux_base` des Manifests geprüft, was `linux_base` erfordert; eine `ET_DYN`-Ausgabe aus `--lazy-binding` erfordert ein Manifest ohne diesen Key. In einer PE-Ausgabe, die ebenfalls ein Manifest ohne `linux_base` erfordert, wird jeder Eintrag an seiner `output_rva` geprüft, die jeder geprüfte Eintrag haben muss; Bytes eines Abschnitts jenseits seiner Dateidaten sind die Nullen, die der Loader dort abbildet, und der Bericht gibt an, wenn ein Bereich nur solche Bytes enthält. Der Eintrag, dessen Ziel `main_target` ist, wird gegen `<path>` geprüft, und Einträge für die angegebenen `--executable`-Strings werden gegen die Ausgabedatei geprüft, die wie bei einer Konvertierung abgeleitet wird; ein Eintrag ist `applied`, wenn die Datei dort `value` enthält. Jeder andere Eintrag ist `not-checked (module, or executable not named with --executable)`, auch ein falsch geschriebenes Modulziel. Zwei Ziele mit demselben Namen, etwa ein `--executable` gleich `main_target`, ergeben den Fehler `ambiguous patch target`. Optionen zur Modulsuche werden mit `--verify-output` abgelehnt.

Der Prüfmodus lehnt die Konvertierungsoptionen (`--windows`, `--macos`, `--to-intel`, `--rpath`, `--patches`, `--no-patches` und ähnliche) und einen zweiten Pfad ab.

## Aufbau zur Laufzeit

Die Windows-Programmdatei hat kein `DYNAMIC_BASE` und wird bevorzugt so geladen, dass ihre niedrigste ELF-Ladeseite bei `0x400000` liegt, wo Ruffle (PPSA68091), VLC (PPSA85300) und XPSemu (PPSA97358) ihr Image erwarten, wenn sie ihre `app0/imports.txt` lesen; die PE-Header belegen die 64 KiB davor (bevorzugte Basis `0x3f0000`). Relokationen bleiben erhalten, sodass Windows die Datei verschieben kann, wenn dieser Bereich belegt ist. Wenn die Programmdatei eine exakte Adresse braucht, verwende `relinker --windows --windows-fixed-base <address> source/input.elf app.exe`; das legt die niedrigste ELF-Ladeseite ohne Relokationen an diese Adresse, sodass das Laden bei belegter Adresse scheitert. Module behalten `DYNAMIC_BASE`. Diese Option ändert die macOS-Ausgabe nicht.

Für eine Programmdatei, die annimmt, dass ihr Image bei `0x400000` beginnt, wie Ruffle (PPSA68091), VLC (PPSA85300) und XPSemu (PPSA97358), wenn sie ihre `app0/imports.txt` lesen, verwende unter macOS `relinker --macos --macos-fixed-base 0x400000 source/input.elf app`. Verwende diese Option nur, wenn die Programmdatei eine feste Adresse braucht; ohne sie legt die macOS-Ausgabe die niedrigste ELF-Ladeseite bei `0x140010000` ab. Die Linux-Ausgabe legt die virtuelle Adresse 0 ohne Option bei `0x400000` ab, innerhalb des 40-Bit-Fensters, das das eigene `sceLibcMspaceCreate` eines Spiels prüft; `--linux-fixed-base` wählt eine andere Adresse, und mit `--lazy-binding` bleibt die Programmdatei positionsunabhängig.

Die Adressen von `--linux-fixed-base` und `--macos-fixed-base` gelten auch für Programmdateien, die mit `--executable` ausgewählt werden; `--windows-fixed-base` gilt nur für die Hauptprogrammdatei, die anderen behalten die bevorzugte Basis mit Relokationen. Mitgelieferte PRX-Module erhalten keine neue Basis.

Pfade sind relativ zur ausgegebenen Programmdatei:

```text
app.elf (Linux) or app.exe (Windows)
libs/
    *.prx
app0/
    <app resources>
    eboot.bin
    sce_module/
        <converted modules>
```

Verwende `sce_modules/` oder `prx/` statt `sce_module/`, wenn das Eingabeverzeichnis so heißt. Der Relinker behält das Verzeichnis jedes Moduls unter `app0/` bei und gibt seinen genauen Pfad aus. Lege die App-Ressourcen separat in `app0/` ab, einschließlich der originalen `eboot.bin` des Spiels: Spiele prüfen `/app0/eboot.bin`, um ihr eigenes Verzeichnis zu finden, da die Datei auf der Konsole immer vorhanden ist (ProsperoTV, ProsperoRadio, ProsperoLight und ProsperoLichess weichen ohne sie auf ein leeres `/data/homebrew/<title id>` aus und können ihre Schriften nicht laden). Nur die konvertierte Programmdatei läuft. Unter Linux wird ein symbolischer Link innerhalb des `app0/`-Baums mit dem Typ der Datei oder des Verzeichnisses aufgeführt, auf das er zeigt, sodass ein verlinktes Verzeichnis wie ein echtes durchlaufen wird. Links innerhalb eines verlinkten Verzeichnisses, deren Ziel außerhalb von `app0/` liegt, behalten den Link-Typ; verlinke bei verschachtelten Bäumen nur die obersten Einträge oder verwende Kopien oder Hardlinks. Kopiere die gebauten Systembibliotheken aus `build/core/libs/libs/*.prx` nach `libs/`; verwende Bibliotheken, die für das Ziel-OS gebaut wurden. Ein eigenes `--rpath` ändert den Ort der Systembibliotheken.

Manche Anwendungen brauchen zur Laufzeit außerdem ihre originale `app0/sce_sys/param.json`. AnyPS5 liest aus dieser Datei die Titel-ID und den Anzeigenamen; sie muss einen nicht leeren String `titleId` und ein nicht leeres Objekt `localizedParameters` mit einem Spracheintrag enthalten, der einen nicht leeren `titleName` hat. Gewählt wird der Eintrag, den `defaultLanguage` nennt (innerhalb von `localizedParameters` oder auf oberster Ebene); fehlt dieser Eintrag, wird `en-US` versucht, dann der erste Spracheintrag, dessen Wert ein Objekt ist. Übernimm die Werte aus den eigenen Metadaten des Spiels und lege die Datei unter demselben `app0/`-Verzeichnis ab wie die übrigen App-Ressourcen. Ist sie beim Start im Arbeitsverzeichnis vorhanden, wird `data/` angelegt; ein `downloadDataSize` ungleich null legt außerdem `download0/` an. Ohne `param.json` liest AnyPS5 stattdessen `app0/sce_sys/param.sfo` (`TITLE`, `TITLE_ID` und `DOWNLOAD_DATA_SIZE`), mit derselben Wirkung auf `data/` und `download0/`; ohne beide Dateien wird keines der beiden Verzeichnisse angelegt. Fehlen beide oder ist die gelesene ungültig, können Metadaten-Aufrufe wie die Einrichtung von Titel und Fenster mit einem Fehler zu `param.json` oder `param.sfo` scheitern.

Ressourcen lassen sich auch in `app0.zar` neben `app0/` packen, ein [ZArchive](https://github.com/Exzap/ZArchive), dessen Wurzel das Verzeichnis `app0/` ist. Eine Datei, die in `app0/` vorhanden ist, wird von dort gelesen, jede andere aus dem Archiv; die Programmdatei, `sce_module/` und `sce_sys/` bleiben in `app0/`. Das Archiv wird nur über `sceKernelOpen`/`open` gelesen, über die Lese-, Seek- und Stat-Aufrufe auf seinen Deskriptoren, `sceKernelStat`, die APR-Dateilesevorgänge (`sceKernelAprResolveFilepathsToIds` und die Lesebefehle), `fopen` zum Lesen, `access` und `sceNgs2ParseWaveformFile`; andere Aufrufe auf einen archivierten Pfad, das Auflisten eines Verzeichnisses, das auch das Archiv enthält, und das Öffnen einer Datei darin zum Schreiben scheitern wie in den [technischen Schulden](../dev/debt/libkernel.md) beschrieben. Ein Archiv, das sich nicht öffnen lässt, beendet das Spiel bei seinem ersten Dateizugriff.

Verwende für mitgelieferte Spielmodule die `.guest.prx`-Dateien, die der Relinker in `app0/` schreibt und im ausgegebenen Aufbau aufführt. `libs/` ist für die Systembibliotheken von AnyPS5 gedacht, nicht für die originalen PS5-`.prx`-Dateien. Liegt ein originales PS5-Modul unter Windows in `libs/`, versucht Windows es als DLL zu laden, was mit Fehler 193 (keine gültige Win32-Anwendung) scheitern kann. Ein Modul, das das Spiel während der Ausführung lädt und das nicht neu gelinkt wurde, etwa eines in einem Verzeichnis, das nicht an `--module-dir` übergeben wurde, wird ebenfalls nicht geladen: Das Spiel erhält einen Fehler und läuft weiter, und die Laufzeitumgebung gibt für jede Datei einmal `[module] dlopen: <path> was not relinked: ...` aus. Linke das Verzeichnis dieses Moduls mit `--module-dir <dir>` neu.

Unter Windows braucht ein selbst gebautes `libs/` außerdem `libgcc_s_seh-1.dll`, `libstdc++-6.dll` und `libwinpthread-1.dll` aus dem Verzeichnis `mingw64/bin` der Toolchain, die die Bibliotheken gebaut hat; die Release-Archive enthalten sie bereits. Die Bibliotheken werden ohne Suche in `PATH` geladen, sodass eine Kopie an anderer Stelle im System nicht verwendet wird, und eine Bibliothek, die eine davon braucht, scheitert mit Fehler 126 (das angegebene Modul wurde nicht gefunden), obwohl ihre `.prx`-Datei vorhanden ist. Ist `APS5_MIMALLOC` in der Umgebung vorhanden, auch mit leerem Wert oder `0`, lädt das Spiel `mimalloc.dll` und `mimalloc-redirect.dll` aus `libs/` und alloziert über mimalloc. Kopiere beide aus `build/core/libs/libs`; die Release-Archive enthalten sie. Ohne sie behält das Spiel den Heap der C-Laufzeitbibliothek und gibt nichts aus.

Unter Linux und macOS aktiviert `APS5_MIMALLOC=1` mimalloc. Ein nicht gesetzter oder leerer Wert, `0`, `false`, `off` oder `no` deaktiviert es. Groß- und Kleinschreibung der Wörter spielt keine Rolle. Andere nicht leere Werte aktivieren es. Entferne `APS5_MIMALLOC` aus der Umgebung, um es auf allen Plattformen zu deaktivieren.

Unter macOS startet das Spiel bei aktiviertem mimalloc sofort neu, mit geladener `libmimalloc.dylib` aus `libs/`, und alloziert über mimalloc; das selbst gebaute `build/core/libs/libs` enthält sie bereits. Ohne die Datei behält das Spiel den System-Allocator und gibt nichts aus.

Unter Linux startet das Spiel bei aktiviertem mimalloc sofort neu, mit `libmimalloc.so` aus `libs/` in `LD_PRELOAD`, und alloziert über mimalloc; die Release-Archive und das selbst gebaute `build/core/libs/libs` enthalten sie. Ohne die Datei behält das Spiel den System-Allocator und gibt nichts aus. Ein `libs/`-Pfad mit Leerzeichen oder Doppelpunkt lässt sich nicht in `LD_PRELOAD` eintragen: Das Spiel gibt dann eine Zeile aus und behält den System-Allocator. Nach dem Neustart bleiben `APS5_MIMALLOC` und `LD_PRELOAD` gesetzt, sodass die eigene Umgebung des Spiels und jeder Prozess, den es startet, sie mitführen. mimalloc behält mehr Speicher als der System-Allocator: In PPSA21564 war damit etwa 0.9 GiB weniger Host-Speicher verfügbar.

Unter Windows wird Direktspeicher (`sceKernelAllocateDirectMemory`, bis zu 13824 MiB pro Spiel) committet, wenn das Spiel ihn mappt, nicht wenn es ihn alloziert oder seine Seiten zum ersten Mal verwendet. Das Commit-Limit des Systems (installierter Arbeitsspeicher plus Größe der Auslagerungsdatei, der zweite Wert unter „Zugesichert“ im Task-Manager) muss den gemappten Direktspeicher zusammen mit allem anderen committeten Speicher abdecken. Andernfalls wirft `sceKernelMapDirectMemory` `replace guest placeholder with resident direct memory` mit dem Windows-Fehler (`commit direct memory pages`, wenn eine Seite an einer zweiten Adresse gemappt wird, `keep unmapped direct memory`, wenn ihr Mapping aufgehoben wird); vergrößere die Auslagerungsdatei oder schließe andere Anwendungen.

Unter Linux wird gemeinsam genutzter Direktspeicher über `/dev/udmabuf` importiert, wenn der Vulkan-Treiber dma-buf-Speicher importiert (nicht beim proprietären NVIDIA-Treiber), und der Benutzer, der das Spiel startet, braucht Lese- und Schreibzugriff darauf. Viele Distributionen legen es als `root:kvm` mit Modus `0660` an: Füge den Benutzer der Gruppe `kvm` hinzu und melde dich neu an (eine ACL wie `setfacl -m u:$USER:rw /dev/udmabuf` hält bis zum nächsten Neustart). Ohne Zugriff gibt der Start `[gpu] open /dev/udmabuf: Permission denied` aus, diese Bereiche werden kopiert statt importiert, und GPU-Schreibzugriffe darauf über FLAT/GLOBAL-Adressen scheitern mit `BDA access failed`. Ein Bereich über 64 MiB wird als ein Sparse-Buffer über udmabuf-Buffern von 64 MiB importiert, wenn das Gerät Sparse-Speicher bindet (RADV, ANV). Ohne Sparse-Binding oder wenn die Sparse-Blockgröße des Geräts über 64 KiB liegt, werden Bereiche oberhalb von `size_limit_mb` von udmabuf (standardmäßig 64 MiB) ebenfalls kopiert.

Linux:

```sh
./app.elf
```

Der Relinker schreibt `app.elf` und jede Datei, die er als `Executable file:` aufführt, mit Ausführungsrecht. Eine Kopie, die ihren Dateimodus verloren hat, etwa ein Browser-Download, braucht zuerst `chmod +x` für jede davon; ohne das endet ein Spiel, das eine andere Programmdatei startet, mit `sceSystemServiceLoadExec: execv failed: Permission denied`.

Windows PowerShell:

```powershell
.\app.exe
```

Eine Windows-Programmdatei, die ohne Handle für Standardausgabe oder Standardfehler gestartet wird, etwa ein aus dem Explorer gestarteter `--windows-gui`-Build, schreibt diese Ausgabe in `anyps5.log` daneben und ersetzt dabei die Datei des vorherigen Laufs. Ohne Schreibzugriff auf dieses Verzeichnis läuft sie ohne Log.

### Anzeigeauflösung und Fenstergröße

Das Spiel wählt seine Auflösung für die Videoausgabe; AnyPS5 verwendet diesen Modus zum Rendern. Das Anzeigefenster lässt sich in der Größe ändern oder in den Vollbildmodus schalten, wodurch das dargestellte Bild auf das Fenster skaliert wird. F11 schaltet zwischen Fenster und Vollbild um und bringt das Fenster auf seine vorherige Größe zurück. Setze `ANYPS5_FULLSCREEN` auf `1` oder `borderless`, um das Fenster von Anfang an im Vollbild mit der Desktop-Auflösung zu öffnen, oder auf `0` oder `windowed` (Standard), um es im Fenster zu lassen. Jeder andere nicht leere Wert, etwa `true` oder `yes`, öffnet es weiterhin wie bisher im Vollbild, und eine Warnung auf stderr nennt den Wert. Das Vollbild erscheint auf dem Bildschirm, auf dem das Fenster liegt, also auf dem, den `ANYPS5_DISPLAY_INDEX` beim Start auswählt. Der Mauszeiger ist im Vollbild ausgeblendet, sofern das Spiel keine Maus geöffnet hat. F2 schaltet das Fenster in den randlosen Fenstermodus: keine Titelleiste und kein Rahmen, so groß, dass es den Bildschirm bedeckt, auf dem es liegt. Randlos ist kein exklusives Vollbild, daher behält der Bildschirm seinen Modus. Ein erneuter Druck auf F2 bringt das Fenster auf seine normale Größe und Position zurück, und F11 verlässt zuerst den randlosen Modus. Setze `ANYPS5_WINDOW_MODE` auf `borderless`, um randlos zu starten; jeder andere Wert oder eine nicht gesetzte Variable startet ein normales Fenster. Ein Vollbildstart über `ANYPS5_FULLSCREEN` hat Vorrang vor `ANYPS5_WINDOW_MODE`. Eine Änderung der Fenstergröße ändert die Render-Auflösung des Spiels nicht. Die einzige Host-Einstellung, die die Auflösung ändert, die ein Spiel liest, ist das unten beschriebene `APS5_OUTPUT_RESOLUTION`; ansonsten bietet AnyPS5 keine Übersteuerung der Render-Auflösung, verwende also die eigene Anzeigeeinstellung eines Spiels, wenn es eine bietet.

`APS5_OUTPUT_RESOLUTION=<width>x<height>`, zum Beispiel `APS5_OUTPUT_RESOLUTION=1920x1080`, lässt den Status der Videoausgabe des Spiels die 1080p-Klasse melden, wenn die Breite unter 3840 und die Höhe unter 2160 liegt, und sonst die 2160p-Klasse, statt der Klasse des aktuellen Modus des Bildschirms. Die beiden Zahlen müssen positive ganze Zahlen sein, getrennt durch `x` oder `X`. Ein leerer Wert gilt wie ein nicht gesetzter; jeder andere Wert beendet das Spiel mit einem Fehler, der die Variable nennt.

`ANYPS5_DISPLAY_INDEX=<n>` öffnet das Fenster zentriert auf Bildschirm `n`, gezählt ab 0 in der Reihenfolge, in der SDL die Bildschirme aufführt. Ohne die Variable wird der erste Bildschirm verwendet. Ein Wert, der nicht der Index eines angeschlossenen Bildschirms ist, auch ein leerer, beendet das Spiel beim Start mit einem Fehler, der die Variable nennt.

Unter Linux fordert das mitgelieferte SDL2 serverseitige Fensterdekorationen an, wenn der Wayland-Compositor xdg-decoration anbietet. Compositoren ohne xdg-decoration, darunter GNOME, verwenden libdecor, wenn es verfügbar ist. Wenn libdecor das Fenster nicht innerhalb von drei Sekunden konfiguriert, versucht AnyPS5 es erneut mit xdg-shell. Der erneute Versuch kann ohne Dekorationen öffnen, und der Start meldet einen Fehler, wenn der Compositor auch dieses Fenster nicht konfiguriert. Setze `SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR=0`, um libdecor zu deaktivieren, `SDL_VIDEO_WAYLAND_PREFER_LIBDECOR=1`, um es zuerst zu versuchen, oder `SDL_VIDEODRIVER=x11`, um X11 oder XWayland zu wählen.

### VR-Kompatibilität

AnyPS5 unterstützt derzeit weder PS VR2-Headsets noch PC-VR-Headsets, die über eine OpenXR-Runtime angeschlossen sind. Es bietet kein Headset-Tracking, keine stereoskopische Ausgabe und keine Reprojektion. Ein VR-Spiel überspringt eventuell seine Headset-Einrichtung, wenn kein Gerät erkannt wird, das bedeutet aber nicht, dass sein VR-Gameplay unterstützt wird.

### Controller-Eingaben für Testläufe

Setze `APS5_PAD_INPUT_FILE` vor dem Start der Anwendung auf einen absoluten Pfad, um Controller-Impulse aus einer lokalen Befehlsdatei zu aktivieren. Sobald die Anwendung Controller-Eingaben liest, sende eine Taste mit dem Hilfsprogramm aus dem Wurzelverzeichnis des Repositorys:

```sh
APS5_PAD_INPUT_FILE=/tmp/anyps5-pad ./app.elf
python3 tools/pad_input.py /tmp/anyps5-pad cross
python3 tools/pad_input.py /tmp/anyps5-pad down --hold-ms 500
python3 tools/pad_input.py /tmp/anyps5-pad release
python3 tools/pad_input.py /tmp/anyps5-pad release --left-stick 128 0 --hold-ms 2000
python3 tools/pad_input.py /tmp/anyps5-pad cross --left-stick 255 128 --right-stick 0 128
python3 tools/pad_input.py /tmp/anyps5-pad touch-right
python3 tools/pad_input.py /tmp/anyps5-pad l1+r1
```

Für zeitgesteuerte Tastenfolgen siehe [Controller-Zeitleisten](INPUT_MAPPING.de.md#controller-zeitleisten).

Starte das Hilfsprogramm in einem zweiten Terminal. Befehle werden während Controller-Lesevorgängen höchstens alle 100 ms abgefragt, und ihre Tasten kommen zu den Eingaben des physischen Controllers hinzu. Ein Tastendruck dauert standardmäßig 300 ms, beginnend mit dem Lesen durch die Anwendung, und wird automatisch losgelassen; `--hold-ms` akzeptiert 1–5000 ms. `--left-stick X Y` und `--right-stick X Y` halten die Sticks für dieselbe Zeit: Jede Achse hat den Bereich 0-255, 128 ist die Mitte, X wächst nach rechts und Y nach unten. Solange ein solcher Befehl aktiv ist, kommen beide Sticks aus ihm statt vom physischen Controller, und ein nicht genannter Stick bleibt in der Mitte. `touch-left` und `touch-right` drücken das Touchpad mit einem Finger auf seiner linken oder rechten Hälfte, wie die Belegungen `TouchLeft` und `TouchRight`; in der Befehlsdatei sind das die Bits `0x200000` und `0x400000` des Tastenfelds. Mit `+` verbundene Tastennamen, etwa `l1+r1`, werden in einem Befehl gleichzeitig gedrückt; `release` lässt sich nicht verbinden. Warte, bis die Anwendung ein Loslassen erkannt hat, bevor du einen weiteren Tastendruck sendest. Die Datei enthält nur den neuesten Befehl; schnell aufeinanderfolgende Befehle können einander ersetzen. Unter POSIX behält das Ersetzen einer vorhandenen Befehlsdatei deren Berechtigungsmodus bei; ein privilegiertes Hilfsprogramm behält auch Besitzer und Gruppe bei, damit der Benutzer des Spiels sie weiterhin lesen kann. Befehle, die beim ersten Lesen des Controllers durch die Anwendung schon vorliegen, werden ignoriert. Ohne die Umgebungsvariable ist die Eingabe über die Datei deaktiviert.

Setze `APS5_NO_PAD_INPUT` auf einen beliebigen Wert, auch einen leeren, damit das Fenster seine Tastatur- und Mausereignisse ignoriert. Die Tastatur- und Mausbelegungen, der Maus-Stick und die Fenstertasten (darunter F10, F11 und F12) bewirken dann nichts. Controller und die Befehle über `APS5_PAD_INPUT_FILE` sind nicht betroffen.

Unter Windows deaktiviert AnyPS5 standardmäßig das DirectInput-Controller-Backend von SDL. Setze `SDL_DIRECTINPUT_ENABLED=1`, um es für Controller zu aktivieren, die DirectInput brauchen. Windows Gaming Input ist standardmäßig aktiviert. Setze `APS5_NO_WGI` auf einen beliebigen Wert, um es beim Start des Videoausgabetreibers zu deaktivieren. Eine ausdrückliche Einstellung von `SDL_JOYSTICK_WGI` hat Vorrang, sodass `SDL_JOYSTICK_WGI=1` es aktiviert lässt.

### Auswahl des Host-Mikrofons

Standardmäßig nimmt `libSceAudioIn` vom Standard-Eingabegerät des Hosts in SDL auf. Setze `ANYPS5_AUDIO_IN_DEVICE` auf den exakten Namen eines Aufnahmegeräts, wie SDL ihn meldet, um ein anderes Mikrofon zu wählen. Ein nicht gesetzter oder leerer Wert oder `default` behält den Standard von SDL bei. Fehlt der gewählte Name, kommt er mehrfach vor oder lässt sich das Gerät nicht öffnen, gibt der AudioIn-Öffnungsaufruf des Spiels den Fehler für ein ungültiges Gerät `0x8026010F` zurück; AnyPS5 wechselt nicht stillschweigend zu einem anderen Mikrofon. SDL2 identifiziert Aufnahmegeräte über ihren Namen, daher lassen sich Geräte mit gleichem Namen nicht einzeln wählen. Die Einstellung wird jedes Mal gelesen, wenn das Spiel einen AudioIn-Port öffnet.

```sh
ANYPS5_AUDIO_IN_DEVICE="USB Microphone" ./app.elf
```

### Vibrationsstärke des Controllers

Setze `ANYPS5_RUMBLE` auf eine ganze Zahl von `0` bis `100`, um zu skalieren, wie stark die Vibration des Spiels deinen Controller erreicht. `100`, auch der Wert bei nicht gesetzter Variable, gibt die Vibration genau so weiter, wie das Spiel sie anfordert. `0` schaltet die Vibration ab: Die Motoren erhalten nie eine Anforderung ungleich null, und eine laufende Vibration wird gestoppt. Werte dazwischen skalieren die Motorstärke um diesen Prozentsatz und gelten für jeden angeschlossenen Controller.

```sh
ANYPS5_RUMBLE=50 ./app.elf
ANYPS5_RUMBLE=0 ./app.elf
```

Die Einstellung umfasst die Vibrationsmotoren, die Impulse-Trigger-Vibration von Controllern, die sie haben, und die Vibration, die ein Spiel als Audio abspielt, sowohl wenn sie auf der eigenen Soundkarte eines DualSense abgespielt als auch wenn sie in Motorvibration umgewandelt wird. Sie ändert nicht die adaptiven Trigger-Effekte des DualSense (Widerstand und Profile für Trigger-Vibration), die weiterhin so beim Controller ankommen, wie das Spiel sie sendet. Ein Wert, der leer ist, ein Vorzeichen oder Nachkommastellen hat oder über 100 liegt, beendet das Spiel beim Start mit einem Fehler, der `ANYPS5_RUMBLE` nennt.

Das ist eine Komforteinstellung auf Host-Seite, kein Verhalten der Konsole: AnyPS5 wendet sie zwischen dem Spiel und deinem Controller an, sie wird einmal beim Start gelesen, und es gibt dafür weder eine Tastenkombination noch einen Overlay-Eintrag. Sie bildet keine Einstellung der Konsole nach.

### Vibration ohne Fensterfokus

Standardmäßig hört der Controller auf zu vibrieren, solange das AnyPS5-Fenster keinen Fokus hat, und vibriert mit dem nächsten Impuls wieder, wenn der Fokus zurückkehrt. Setze `ANYPS5_RUMBLE_ON_FOCUS_LOSS=0`, damit die Vibration an bleibt, während das Fenster im Hintergrund ist. Betroffen sind nur die Vibrationsmotoren: Die Lightbar und der Zustand des Controllers werden nicht geändert, und Erkennungsimpulse funktionieren weiterhin, solange das Fenster den Fokus hat. Jeder andere Wert als `0` oder `1` beendet das Spiel beim Start mit einem Fehler, der die Variable nennt.

### Controller-Verbindung und Benutzer 0

Standardmäßig bleibt Port 0 verbunden und der erste Benutzer angemeldet, auch wenn der einzige Controller entfernt wird, sodass das Spiel weiter den letzten Zustand liest und nie erfährt, dass das Pad fehlt. Setze `ANYPS5_PAD_PORT0_STATE=controller`, damit das Spiel stattdessen den Controller sieht:

```sh
ANYPS5_PAD_PORT0_STATE=controller ./app.elf
```

Mit `controller` sieht das Spiel beim Entfernen des Controllers, der Spieler 1 zugeordnet ist, ein getrenntes Pad mit genullten Eingaben, und Benutzer 0 meldet sich ab; beim erneuten Anschließen wird das Pad verbunden und der Benutzer wieder angemeldet. Eine Änderung wird erst übernommen, wenn der Controller 500 ms lang weg oder vorhanden war, sodass ein Kabel, das kurz aussetzt und zurückkommt, das Spiel nicht hin- und herschalten lässt. Tastatur und Maus zählen in diesem Modus nicht als Controller: Solange Spieler 1 kein Controller zugeordnet ist, werden ihre Eingaben nicht an das Spiel weitergegeben. Der Wert `default` oder eine nicht gesetzte Variable behält das oben beschriebene Verhalten bei. Jeder andere Wert, auch ein leerer, beendet das Spiel beim Start der Videoausgabe mit einem Fehler, der `ANYPS5_PAD_PORT0_STATE` nennt.

Nur Port 0 und Benutzer 0 folgen dem Controller. Die Spieler 2 bis 4 melden weiterhin den Zustand wie bisher.

### Erkennungsimpuls des Controllers

Setze `ANYPS5_PAD_IDENTIFY`, um zu spüren, welcher physische Controller welcher Spieler ist. `off`, der Standard, sendet nichts. `connect` sendet einem Controller ein kurzes Muster, wenn er einem Spielerplatz zugeordnet wird: einen Impuls für Spieler 1, zwei für Spieler 2, bis zu vier für Spieler 4. `always` sendet dasselbe Muster außerdem alle 30 Sekunden erneut, solange das Spiel nicht vibriert. Das Muster beginnt, sobald das Spiel 500 ms lang nicht vibriert hat. Beginnt das Spiel zu vibrieren, bricht der Rest des Musters sofort ab, und die Vibration des Spiels wird wie gewohnt gesendet. Das Muster wird mit `ANYPS5_RUMBLE` skaliert, ein Wert von `0` sendet nichts. Ein Controller ohne Vibrationsunterstützung schreibt eine Zeile ins Log und erhält keinen Impuls. Tastatur und Maus erhalten nie einen Impuls. Ein anderer Wert als `off`, `connect` oder `always`, auch ein leerer, beendet das Spiel beim Start mit einem Fehler, der die Variable nennt.

```sh
ANYPS5_PAD_IDENTIFY=connect ./app.elf
```

Das ist eine Komfortfunktion auf Host-Seite, kein Verhalten der Konsole: Das Spiel sendet diese Impulse nicht. Zeitverlauf und Stärke der Impulse hat AnyPS5 festgelegt; sie wurden nicht an echten Controllern gemessen.

### Telemetrie für Video-out und Pad

Setze `APS5_VIDEOOUT_TELEMETRY_DIR` vor dem Start der Anwendung auf ein Verzeichnis, um die Presents und Pad-Änderungen des Laufs nach `<directory>/videoout.jsonl` zu schreiben, ein JSON-Objekt pro Zeile, angehängt, wenn die Datei schon existiert. Jede Zeile beginnt mit `"v":1`, den Nanosekunden der Steady Clock des Hosts in `t_ns` und dem Ereignisnamen in `ev`. Ein Lauf beginnt mit einer `contract`-Zeile, `{"v":1,"t_ns":<ns>,"ev":"contract","contract":"v1-pad","commit":"<12 hex>","pid":<pid>,"prx":"libSceVideoOut"}`: `commit` ist der Quell-Commit, aus dem die Bibliotheken gebaut wurden (`unknown`, wenn Git bei der Konfiguration nicht gefunden wurde), und `pid` unterscheidet die Läufe, die an dieselbe Datei angehängt haben. Eine `pad`-Zeile wird geschrieben, wenn sich der Pad-Zustand, den das Spiel liest, in Tasten, Sticks oder Triggern ändert, `{"v":1,"t_ns":<ns>,"ev":"pad","n":<present>,"buttons":<bits>,"sticks":[<left x>,<left y>,<right x>,<right y>],"triggers":[<L2>,<R2>]}`, wobei `n` die Anzahl der abgeschlossenen Presents ist, als die Änderung veröffentlicht wurde, also der Zähler, den der Fenstertitel zeigt. `buttons` ist das Tasten-Bitfeld in Dezimaldarstellung, die Stick-Achsen reichen von 0-255 mit 128 in der Mitte, und beide Trigger reichen von 0-255. Bewegung, Touch und Geräteart werden nicht verglichen, sodass ein Lauf, der den Controller nur bewegt, keine `pad`-Zeile schreibt. Der Zustand, den eine `pad`-Zeile meldet, sind die über SDL gelesenen Eingaben von Controller und Tastatur/Maus; Eingaben aus `APS5_PAD_INPUT_FILE` werden erst später angewendet, in `libScePad`, sodass ein geskripteter Lauf keine `pad`-Zeile schreibt. Eine `present`-Zeile wird nach dem ersten Present und danach alle 30 Presents geschrieben, `{"v":1,"t_ns":<ns>,"ev":"present","n":<present>,"fps":<rate>}`, mit dem Zähler und der Rate, die der Fenstertitel zeigt. Ein Hintergrund-Thread schreibt die Zeilen, und das Spiel wartet nicht auf ihn: Fällt er zurück, wird die Zeile verworfen, und eine `drops`-Zeile mit der Anzahl der bisher verworfenen Zeilen, `{"v":1,"t_ns":<ns>,"ev":"drops","dropped":<count>}`, wird alle zehn Sekunden geschrieben, solange die Anzahl wächst, sowie einmal bei einem normalen Beenden, nur wenn Zeilen verworfen wurden; ein Beenden, das statische Destruktoren überspringt, schreibt keine letzte Zeile. Eine Pad-Zeile, die die volle Warteschlange abgewiesen hat, zählt als verworfen, auch wenn eine spätere Veröffentlichung die Änderung schreibt. Ohne die Variable wird keine Datei geöffnet und kein Thread gestartet; ein leerer Wert gilt wie ein nicht gesetzter, und ein Verzeichnis, das sich nicht beschreiben lässt, beendet das Spiel beim Start der Videoausgabe mit einem Fehler, der die Datei nennt.

Um die Kadenz abgeschlossener Guest-Flips zu messen, setze `APS5_VIDEOOUT_FLIP_TIMING=FIRST:COUNT` und verwende für jeden Lauf ein frisches `APS5_VIDEOOUT_TELEMETRY_DIR`. Zum Beispiel zeichnet `601:600` die Flips 601 bis 1200 auf. Die erste Flip-Nummer ist positiv und die Anzahl 2–100000. Die Aufzeichnung verwendet die erste Hauptausgabe, die einen Flip abschließt. Der Contract erhält zusätzlich `flip_start` und `flip_count`, und jede `flip_complete`-Zeile gibt die Abschlussnummer `n`, die `generation` der Ausgabe und den Zeitstempel `process_us` des Guest-Flip-Status in monotonen Mikrosekunden seit Prozessstart an. Benachbarte `process_us`-Werte messen den Abstand zwischen abgeschlossenen Flips, einschließlich leerer Frames.

Verwende eine geordnete, lückenlose Folge, die das gesamte Fenster des Contracts in einer einzigen Generation abdeckt. Fehlende Datensätze weisen auf eine unvollständige Aufzeichnung hin, etwa nach einem Verwerfen in der Warteschlange, einem erneuten Öffnen der Ausgabe oder einem vorzeitigen Beenden. Verwende das vorhandene `drops`-Ereignis, um verlorene Telemetrie-Datensätze zu untersuchen.

### Ersatzton für den Controller-Lautsprecher

Setze `APS5_PAD_SPEAKER_FALLBACK_DB` vor dem Start der Anwendung, um die Verstärkung für AudioOut2-Ports des Controller-Lautsprechers zu ändern, die in die Stereo-Hauptausgabe eingemischt werden, wenn kein Controller-Lautsprecher vorhanden ist. Der Wert wird einmal gelesen, wenn ein Spiel seinen AudioOut2-Kontext anlegt. Standard ist `0` dB, was den Mix unverändert lässt. Akzeptiert wird eine Zahl von `-60` bis `0` oder `mute`, `off` bzw. `-inf` für Stille; umgebende Leerzeichen werden ignoriert. Jeder andere Wert, auch ein leerer, eine Zahl außerhalb des Bereichs, eine nicht endliche Zahl oder nachfolgender Text, beendet das Spiel mit einem Fehler, der die Variable und den Wert nennt. Die Einstellung wirkt sich nicht auf Ports des Hauptmixes oder einen echten Controller-Lautsprecher aus und gilt nicht für den Controller-Lautsprecher-Port des älteren AudioOut.

Ein Spiel, das einen Ton sowohl auf einem Haupt-Port als auch auf einem Controller-Lautsprecher-Port abspielt, behält die Kopie des Haupt-Ports, wenn der Ersatzton stummgeschaltet ist. Diese Kopie folgt den eigenen Lautstärkeeinstellungen des Spiels, die Kopie für den Controller-Lautsprecher womöglich nicht.

### Systemschriften

Spiele, die die Systemschriftsätze der Konsole öffnen (`sceFontOpenFontSet`), brauchen Schriftdateien in einem Verzeichnis `anyps5-fonts/` neben der ausgegebenen Programmdatei; setze `ANYPS5_SYSTEM_FONTS`, um ein anderes Verzeichnis zu verwenden. Von der Konsole gedumpte Dateien werden unter ihren eigenen Namen verwendet (`SST-Roman.otf`, `SST-Bold.otf`, `SSTJpPro-Regular.otf`, ...). Ohne sie werden, sofern vorhanden, diese frei lizenzierten Ersatzschriften verwendet: `NotoSans-{Light,Regular,Medium,Bold}.ttf` und `NotoSans-{LightItalic,Italic,MediumItalic,BoldItalic}.ttf` (Lateinisch und Vietnamesisch), `NotoSansMono-{Light,Regular,Medium,Bold}.ttf` (Schreibmaschine), `NotoSansThai-{Light,Regular,Medium,Bold}.ttf` (Thai), `NotoSansArabic-{Light,Regular,Medium,Bold}.ttf` (Arabisch) und `NotoSansCJK-{Light,Regular,Medium,Bold}.ttc` (Japanisch, Chinesisch und Koreanisch). Ein Satz, der mehrere Schriftsysteme abdeckt, öffnet für die Zeichen, die seiner Hauptdatei fehlen, auch die Dateien seiner anderen Schriftsysteme (`SSTThai-*.otf`, `SSTArabic-*.otf`, `SSTJpPro-*.otf`, `DFHEI5-SONY.ttf`, `SCEPS4Yoongd-*.otf` oder deren Ersatz); fehlt eine davon, bleiben diese Zeichen ohne Glyphe. Ohne beides scheitert das Öffnen eines Systemschriftsatzes (das Log zeigt `sceFontOpenFontSet` mit `file=(none)`), und was dann passiert, hängt vom Spiel ab: Manche zeigen in diesen Schriften keinen Text, andere enden beim Start, weil sie per Assertion prüfen, dass das Öffnen gelungen ist. Die Ersatzschriften stammen aus dem [Noto-Projekt](https://github.com/notofonts/notofonts.github.io) unter der SIL Open Font License (die Noto-Sans-Dateien liegen in `fonts/NotoSans/full/ttf`, die arabischen in `fonts/NotoSansArabic/full/ttf`); kopiere die oben genannten Dateien nach `anyps5-fonts/`. Wo das Noto-Paket einer Linux-Distribution installiert ist, lässt es sich auch ohne Kopieren verwenden: Richte `ANYPS5_SYSTEM_FONTS` auf das Verzeichnis, das `NotoSans-Regular.ttf` enthält (zum Beispiel `/usr/share/fonts/noto` aus `noto-fonts` von Arch Linux). Dieses Verzeichnis enthält nur die Dateien eines Pakets, und `ANYPS5_SYSTEM_FONTS` nennt ein einziges Verzeichnis: Arch installiert die `NotoSansCJK-*.ttc`-Dateien aus `noto-fonts-cjk` in `/usr/share/fonts/noto-cjk`, sodass die japanischen, chinesischen und koreanischen Sätze dort keine Datei haben. Um sie abzudecken, verlinke oder kopiere die Dateien beider Pakete in ein Verzeichnis und richte `ANYPS5_SYSTEM_FONTS` darauf.

### Konsolensprache

Spiele fragen die Sprache der Konsole mit `sceSystemServiceParamGetInt` ab und zeigen ihren Text darin an, wenn sie ihn in dieser Sprache haben. Sie ist Englisch (USA), sofern `ANYPS5_LANGUAGE` nicht auf eine andere Sprachnummer der Konsole gesetzt ist: 0 Japanisch, 1 Englisch (USA), 2 Französisch, 3 Spanisch, 4 Deutsch, 5 Italienisch, 6 Niederländisch, 7 Portugiesisch (Portugal), 8 Russisch, 9 Koreanisch, 10 Chinesisch (traditionell), 11 Chinesisch (vereinfacht), 12 Finnisch, 13 Schwedisch, 14 Dänisch, 15 Norwegisch, 16 Polnisch, 17 Portugiesisch (Brasilien), 18 Englisch (Vereinigtes Königreich), 19 Türkisch, 20 Spanisch (Lateinamerika), 21 Arabisch, 22 Französisch (Kanada), 23 Tschechisch, 24 Ungarisch, 25 Griechisch, 26 Rumänisch, 27 Thai, 28 Vietnamesisch, 29 Indonesisch, 30 Ukrainisch. Zum Beispiel wählt `ANYPS5_LANGUAGE=21` Arabisch. `ANYPS5_LANGUAGE=auto` (in beliebiger Groß- und Kleinschreibung) übernimmt die Sprache stattdessen vom Betriebssystem des Hosts: unter Linux die erste von `LC_ALL`, `LC_MESSAGES` und `LANG`, die gesetzt und nicht leer ist, mit den Einträgen von `LANGUAGE` davor, wenn diese Locale nicht `C` oder `POSIX` ist, unter Windows den Locale-Namen des Benutzers und unter macOS die Liste der bevorzugten Sprachen. Die regionalen Varianten, die die Konsole hat, bleiben erhalten (`en-GB` ist Englisch (Vereinigtes Königreich), `fr-CA` Französisch (Kanada), `es-MX` Spanisch (Lateinamerika), `pt-PT` Portugiesisch (Portugal), `zh-TW`, `zh-HK` und `zh-Hant` Chinesisch (traditionell), `zh-CN` und `zh-Hans` Chinesisch (vereinfacht)), und eine Sprache ohne Eintrag für die Region verwendet ihre Hauptvariante. Eine Host-Sprache, für die die Konsole keinen Eintrag hat, sowie die Locales `C` und `POSIX` ergeben Englisch (USA) und geben auf stderr eine Zeile aus, die die Locale nennt. Der Host wird einmal pro Prozess gelesen, danach beantwortet jede Abfrage dieselbe Sprache. Ein leerer Wert gilt wie ein nicht gesetzter; jeder andere Wert beendet das Spiel bei seiner ersten Sprachabfrage mit einem Fehler, der die Variable nennt.

### Freier Speicher für Spielstände

Standardmäßig wird ein Spielstand nie wegen fehlenden Speicherplatzes abgelehnt, und die freien Blöcke, die das Spiel sieht, sind die seines eigenen Spielstands. Setze `ANYPS5_FREE_SPACE=host`, damit ein neuer Spielstand nicht angelegt wird, wenn das Volume mit den Spielständen weniger Platz hat als die Größe des Spielstands, sodass das Spiel melden kann, dass der Speicher voll ist. Ein Spielstand mit 16384 Blöcken, der Größe, die verwendet wird, wenn das Spiel keine angibt, braucht 1 GiB. Die freien Blöcke, die ein Spiel für einen Spielstand liest, sind dann auf den Platz begrenzt, den sein Volume noch hat. `fixed`, der Wert bei nicht gesetzter Variable, behält das bisherige Verhalten bei, und jeder andere Wert, auch ein leerer, beendet das Spiel beim nächsten Einhängen eines Spielstands oder bei der nächsten Spielstandsuche, die einen Spielstand findet, mit einem Fehler, der `ANYPS5_FREE_SPACE` nennt. Unter Windows zählt ein leerer Wert als nicht gesetzt, weil `set ANYPS5_FREE_SPACE=` in `cmd` und `$env:ANYPS5_FREE_SPACE = ""` in PowerShell die Variable entfernen. Vorhandene Spielstände werden in beiden Modi wie bisher geöffnet. Ob eine Konsole einen Spielstand auf dieselbe Weise ablehnt, ist nicht gemessen; siehe die [technischen Schulden](../dev/debt/libSceSaveData.native.md) von `libSceSaveData.native`.

### Spielmodus des Hosts

Setze `APS5_GAME_MODE=on`, um beim Host für die Laufzeit des Spiels dessen Spiel- oder Leistungsmodus anzufordern. Standard ist `off`, was den Host unberührt lässt: Nichts wird geladen und nichts angefordert.

Unter Linux geht die Anforderung an [Feral GameMode](https://github.com/FeralInteractive/gamemode): AnyPS5 lädt beim Start `libgamemode.so.0` (danach `libgamemode.so`) mit `dlopen`, bittet `gamemoded`, den Spielmodus zu starten, und beendet die Anforderung, wenn das Spiel endet. Was `gamemoded` dann ändert (CPU-Governor, Prozesspriorität, GPU-Einstellungen), kommt aus dessen eigener Konfiguration, `gamemode.ini`, nicht von AnyPS5. Unter macOS hält es eine vom Benutzer ausgelöste, latenzkritische `NSProcessInfo`-Aktivität, die App Nap und Timer Coalescing vom Prozess fernhält. Unter Windows gibt es nichts anzufordern: AnyPS5 bittet Windows bereits beim Laden, die Timer des Prozesses nicht zu drosseln.

`APS5_GAME_MODE=auto` stellt dieselbe Anforderung, meldet aber nichts, wenn der Host sie nicht annehmen kann (keine GameMode-Bibliothek, kein Foundation). Mit `on` wird eine fehlende Bibliothek einmal als `[host-performance]` ins Log geschrieben. In beiden Modi wird eine Anforderung, die der Daemon ablehnt, etwa weil `gamemoded` nicht läuft, einmal ins Log geschrieben, und das Spiel läuft in jedem Fall weiter. Jeder andere Wert beendet das Spiel beim Start mit einem Fehler, der `APS5_GAME_MODE` nennt.

### Add-ons

Spiele fragen mit `sceNpEntitlementAccessGetAddcontEntitlementInfo` ab, ob ein Add-on (herunterladbarer Inhalt) installiert ist, und listen die installierten mit `sceNpEntitlementAccessGetAddcontEntitlementInfoList` auf. Ein Add-on gilt nur dann als installiert, wenn sein Entitlement-Label in `anyps5-entitlements.ini` neben der ausgegebenen Programmdatei oder in der Datei steht, die `ANYPS5_ENTITLEMENTS` nennt: ein Label pro Zeile, bis zu 16 Zeichen, `#` oder `;` leitet einen Kommentar ein. Trage die Add-ons ein, die deine Kopie enthält. Jedes andere Label wird als nicht installiert gemeldet, und die erste Abfrage für jedes davon gibt eine Zeile aus, etwa `[entitlements] the title asked for the add-on EXAMPLEADDON0000, which anyps5-entitlements.ini does not list, so it is reported as not installed`. Die Zeile nennt die Datei als `anyps5-entitlements.ini` oder mit dem Pfad, den `ANYPS5_ENTITLEMENTS` angibt, wenn die Variable gesetzt ist. Bytes eines Labels außerhalb von druckbarem ASCII sowie der Backslash werden als `\xNN` geschrieben. Nach 64 solchen Labels sagt eine weitere Zeile, dass weitere ohne Zeile gemeldet werden. Manche Spiele warten beim Start auf ein Add-on, nach dem sie suchen, statt ohne es weiterzumachen: Ein Spiel (PPSA26344, Version 02.002.000) bleibt bei seinem Lade-Spinner, bis das Add-on-Label, nach dem es fragt, eingetragen ist.

### Links im Browser des Hosts öffnen

Ein Spiel, das mit `sceSystemServiceLaunchWebBrowser` eine Webseite anfordert, erhält einen Fehler, und nichts öffnet sich, sofern `ANYPS5_OPEN_URLS` nicht gesetzt ist. `ANYPS5_OPEN_URLS=ask` zeigt die vollständige Adresse in einem Meldungsfenster mit Cancel als Standardschaltfläche und öffnet sie nur dann im Standardbrowser, wenn du Open drückst; ohne Anzeige für das Fenster öffnet sich nichts. `ANYPS5_OPEN_URLS=always` öffnet sie ohne Rückfrage, was nicht empfohlen ist, weil das Spiel die Adresse wählt. Anders als bei `ask`, wo du die Adresse zuerst siehst, öffnet `always` jeden `http`- oder `https`-Host, den das Spiel nennt, auch `localhost` und Adressen in privaten Netzen wie die Weboberfläche eines Routers. `never`, ein leerer Wert und eine nicht gesetzte Variable behalten den Fehler bei. Jeder andere Wert beendet das Spiel bei seiner ersten Browser-Anfrage mit einem Fehler, der die Variable nennt.

Browser-Anfragen werden für alle Adressen und alle Threads des Spiels gemeinsam begrenzt: Eine Anfrage, die eintrifft, während noch ein Fenster oder ein Start aktiv ist, wird sofort abgelehnt, in je 60 Sekunden gibt es höchstens drei Fenster oder Starts, und nachdem du Abbrechen gedrückt hast, gibt es 10 Minuten lang kein weiteres Fenster und keinen weiteren Start. Abgelehnte Anfragen scheitern wie im Standardfall.

Nur `http://`- und `https://`-Adressen mit höchstens 2048 Bytes werden geöffnet. Der Host lehnt Adressen mit Benutzername oder Passwort, einem Leerzeichen, einem Steuerzeichen, einem Zeichen außerhalb von ASCII (verwende Punycode und Prozent-Escapes), einem fehlerhaften oder ein Steuerzeichen kodierenden Prozent-Escape, einem leeren Host oder einem Port außerhalb von 1 bis 65535 sowie `psno:`-Adressen ab. Der Browser wird ohne Shell gestartet: `xdg-open` unter Linux, `open` unter macOS und `ShellExecuteW` unter Windows.

### Datums- und Zeitformat der Konsole

Spiele fragen mit `sceSystemServiceParamGetInt` auch das Datums- und das Zeitformat der Konsole ab. Ohne Einstellung ist das Datumsformat Tag, Monat, Jahr und das Zeitformat 24 Stunden. `ANYPS5_DATE_FORMAT` setzt das Datumsformat auf 0 (Jahr, Monat, Tag), 1 (Tag, Monat, Jahr) oder 2 (Monat, Tag, Jahr), und `ANYPS5_TIME_FORMAT` setzt das Zeitformat auf 0 (12 Stunden) oder 1 (24 Stunden). Zum Beispiel ergibt `ANYPS5_DATE_FORMAT=2` mit `ANYPS5_TIME_FORMAT=0` Monat, Tag, Jahr und eine 12-Stunden-Uhr. Beide Variablen akzeptieren auch `auto` (in beliebiger Groß- und Kleinschreibung), das die Wahl aus den kurzen Datums- und Zeitmustern des Hosts übernimmt: unter Linux die Locale, die die erste von `LC_ALL`, `LC_TIME` und `LANG` nennt, die gesetzt und nicht leer ist (`LANGUAGE` und `LC_MESSAGES` zählen nicht), unter Windows die kurzen Datums- und Zeitformate des Benutzers aus den Regionseinstellungen und unter macOS die aktuelle Locale. Verwendet werden nur die Reihenfolge von Tag, Monat und Jahr und ob die Uhr 12 oder 24 Stunden hat, Trennzeichen und Namen nicht. Wenn der Host keine Vorgabe hat (nichts gesetzt, die Locales `C` und `POSIX`, eine nicht installierte Locale) oder sein Datumsmuster eine Reihenfolge hat, für die die Konsole kein Format hat, bleibt der Wert 1, und eine Zeile auf stderr nennt die Variable. Der Host wird einmal pro Prozess gelesen, sodass danach jede Abfrage dasselbe beantwortet. Zeitzone und Sommerzeit folgen bereits dem Host. Ein leerer Wert gilt wie ein nicht gesetzter; jeder andere Wert beendet das Spiel bei seiner ersten Abfrage dieses Parameters mit einem Fehler, der die Variable nennt.

### Trophäen

Ein Spiel schaltet eine Trophäe frei, indem es ein `_UnlockTrophy`-Ereignis mit einer int32-Eigenschaft `_trophy_id` an `libSceNpUniversalDataSystem` sendet, oder über die unten beschriebenen Statistikregeln. Die Freischaltung wird in `_trophy/<user id>_<service label>.txt` in dem Verzeichnis gespeichert, in dem das Spiel läuft (dem, das `app0/` enthält), eine Zeile `<trophy id> <unlock time>` pro Trophäe; die Zeit ist ein RTC-Tick, Mikrosekunden seit 0001-01-01 UTC. Lösche die Datei vor dem Start des Spiels, um seine Trophäen wieder zu sperren. Jede neue Freischaltung gibt eine Zeile aus, etwa `[trophy] PPSA01556 unlocked trophy 12 for user 268435456`; eine erneute Freischaltung gibt nichts aus. `sceNpTrophy2GetTrophyInfo`, `sceNpTrophy2GetTrophyInfoArray` und `sceNpTrophy2GetGameInfo` melden dem Spiel die gespeicherten Freischaltungen und ihre Zeiten. Wenn die Trophäendaten des Spiels geladen sind (siehe unten), wird eine gesendete Freischaltung einer Trophäe, die in den Daten fehlt, der Platin-Trophäe oder einer progressiven Trophäe nicht gespeichert und gibt den Grund aus, etwa `[trophy] ignored the posted unlock of trophy 0 for user 268435456: it is the platinum, which unlocks with the last trophy that names it`; die Platin-Trophäe wird mit der letzten Trophäe freigeschaltet, die sie nennt, eine progressive Trophäe über ihre Statistik. Ohne Trophäendaten wird jede gesendete ID ab 0 gespeichert, und die Platin-Trophäe wird nicht freigeschaltet, wenn die übrigen Trophäen es sind.

Die Trophäendaten stammen aus der eigenen `app0/sce_sys/trophy2/trophyNN.ucp` des Spiels, wobei `NN` das Service-Label ist (`00` bei den meisten Spielen). Die Trophäenabfragen melden ihre Namen, Beschreibungen und Belohnungen in der Konsolensprache (`ANYPS5_LANGUAGE`, siehe oben) oder in Englisch (USA) bzw. der Standardsprache der Daten, wenn sie in dieser Sprache keinen Text haben, sowie ihre Stufen, Gruppen, Versteckt-Kennzeichen, die Anzahl der Trophäen und die Anzahl je Stufe. Eine Platin-Trophäe wird freigeschaltet, sobald jede Trophäe freigeschaltet ist, die sie in `platinumTrophyId` nennt. Die erste Trophäenabfrage oder das erste Ereignis gibt aus, was gelesen wurde, etwa `[trophy] /app0/sce_sys/trophy2/trophy00.ucp: 52 trophies, 1 group, texts in en-US`.

Viele Spiele schalten Trophäen stattdessen über Statistiken frei: `app0/sce_sys/uds/udsNN.ucp` nennt für jedes Ereignis die Eigenschaft, deren Wert zu einer Statistik wird, und die `unlockCondition` einer Trophäe vergleicht eine Statistik mit einem Zielwert. Der neueste Wert jeder Statistik wird in `_trophy/<user id>_<service label>.stats.txt` gespeichert, eine Zeile `<stat id> <value>` pro Statistik, und bleibt über Läufe hinweg erhalten; lösche die Datei zusammen mit der Freischaltungsdatei, um von vorn zu beginnen. Eine progressive Trophäe meldet ihre Statistik als Fortschritt. Angewendet werden nur Statistiken, die den neuesten (`"aggregation":"latest"`) oder den größten Wert (`"max"`) als `uint32`, `uint64` oder `int32` behalten, und Regeln, die eine Eigenschaft eines anhand seines Namens gewählten Ereignisses kopieren (`$.name` oder `$.object.name`), wahlweise nur, wenn eine Eigenschaft einer ganzen Zahl entspricht (`_UpdateTrophyProgress` über seine `_trophy_id`); die übrigen werden beim Lesen der Datei in `[trophy]`-Zeilen aufgeführt.

`APS5_TRACE_UDS=1` gibt jedes gesendete Ereignis aus, etwa `[uds] context 1 posted _UnlockTrophy {"_trophy_id":12}`, jede Statistik, die ein Ereignis setzt, etwa `[uds] stat 10 of user 268435456 is 10 (COLLECT $.counter)`, und eine `[uds]`-Zeile für eine fehlende `trophyNN.ucp` oder `udsNN.ucp`. Ein Ereignis, das nicht verwendet wird, gibt den Grund in einer `[trophy] ignored`-Zeile aus: ein `_UnlockTrophy`-Ereignis ohne int32-`_trophy_id` ab 0 oder eines, das an einen nicht vorhandenen Kontext gesendet wurde; und, einmal pro Regel, ein Ereignis, dessen Eigenschaft fehlt, negativ ist, keine ganze Zahl ist oder außerhalb des Bereichs der Statistik liegt.

Eine fehlende oder fehlerhafte `trophyNN.ucp` oder `udsNN.ucp` schaltet nur ab, was sie bereitstellt, und eine fehlerhafte gibt den Grund einmal aus: Ohne Trophäendaten melden die Abfragen Platzhalter und Statistiken schalten nichts frei, und ohne Statistikregeln ändern Ereignisse keine Statistik. Symbole werden nicht gelesen: `sceNpTrophy2GetGameIcon`, `sceNpTrophy2GetGroupIcon` und `sceNpTrophy2GetTrophyIcon` beenden das Spiel mit einem Fehler, und `sceNpTrophy2GetRewardIcon` gibt `SCE_NP_TROPHY2_ERROR_ICON_FILE_NOT_FOUND` zurück. Dasselbe gilt für eine Freischaltungs- oder Statistikdatei in `_trophy/`, die beschädigt ist (eine Zeile, die kein neues `<id> <value>`-Paar ist, etwa eine Zeile, die eine Bearbeitung leer gelassen oder mit einem Wagenrücklauf beendet hat) oder sich nicht lesen oder schreiben lässt. Es wird keine Benachrichtigung angezeigt, und mit `sceNpTrophy2RegisterUnlockCallback` registrierte Callbacks werden nicht aufgerufen.

### PlayGo-Dumps ohne Metadaten

Wenn ein Dump weder `sce_sys/playgo-chunk.dat` noch `sce_sys/playgo-scenario.json` enthält, kann AnyPS5 nicht erkennen, welche PlayGo-Chunk-IDs ungleich null existieren. Wenn `sce_sys` überhaupt keine `playgo-`-Datei enthält, melden die Chunk-IDs 0 bis 999 standardmäßig „installiert“ (`LOCAL_FAST`), und IDs ab 1000 geben `SCE_PLAYGO_ERROR_BAD_CHUNK_ID` zurück; die Grenze von 1000 stammt aus Läufen von Spielen, nicht von einer Konsole. Setze `APS5_PLAYGO_UNLISTED_CHUNKS`, um für diesen Dump eine andere Antwort zu wählen:

| Wert | Antwort für eine nicht aufgeführte Chunk-ID | Beobachtetes Spiel, das ihn braucht |
|-------|---------------------------------|------------------------------|
| `installed` | Als installiert melden (`LOCAL_FAST`). | Keines bekannt: High on Life (PPSA15388), das ihn zum Laden seiner Spielstände brauchte, fragt nur die IDs 0 bis 999 ab, die der Standard beantwortet. |
| `absent` | `SCE_PLAYGO_ERROR_BAD_CHUNK_ID` zurückgeben. | Sifu (PPSA03001), Demon Slayer: Kimetsu no Yaiba - The Hinokami Chronicles (PPSA03490) und Guilty Gear -Strive- (PPSA02257) beenden mit diesem Ergebnis ihre Suche nach Chunk-IDs. Mit `installed` fragt PPSA03490 endlos nach höheren Chunk-IDs, sodass seine Chunk-Liste wächst, bis der Start mit dem Fehler `Ran out of memory allocating 5565054976 bytes` scheitert. PPSA02257 scheitert auf dieselbe Weise nach etwa 245 Millionen `scePlayGoGetLocus`-Aufrufen in 30 Sekunden. |
| Eine kommagetrennte Liste von IDs, z. B. `0,1` | Diese IDs als installiert melden (`LOCAL_FAST`) und jede andere nicht aufgeführte ID als fehlend (`SCE_PLAYGO_ERROR_BAD_CHUNK_ID`). | The Medium (PPSA03718) braucht die Chunks 0 und 1; mit `installed` fragt es nach jeder ID, und UE4 geht der Speicher aus. |

Lass die Variable ungesetzt oder leer, um den obigen Standard beizubehalten. Ist `sce_sys/playgo-scenario.json` die einzige Chunk-Metadatei, sind die IDs 0 bis 999 aufgeführt. Für IDs ab 1000 meldet `installed` den Chunk als installiert, und `absent` gibt `SCE_PLAYGO_ERROR_BAD_CHUNK_ID` zurück. Ein nicht gesetzter oder leerer Wert oder eine Liste von IDs endet für diese Chunks weiterhin mit einem Fehler.

Eine Liste enthält dezimale Chunk-IDs von 0 bis 65535 und wird zu Chunk 0 hinzugefügt. Sie gilt nur, wenn dem Dump `app0/playgo-chunkdefs.xml` fehlt und `sce_sys` keine `playgo-`-Datei enthält. Andere Werte enden bei Verwendung mit einem Fehler. Wenn `app0/playgo-chunkdefs.xml` oder `sce_sys/playgo-chunk.dat` die Chunk-Liste liefert, hat die Variable keine Wirkung. Diese spielspezifischen Ergebnisse stammen aus Emulatorläufen und wurden nicht an einer Konsole überprüft.

### Screenshots

Drücke F12 im Spielfenster, um den nächsten angezeigten Frame als PNG in der Auflösung des Anzeigepuffers des Spiels zu speichern, ohne die Skalierung des Fensters, Letterbox oder [Leistungs-Overlay](#leistungs-overlay). Ein BT.2100-PQ-Anzeigepuffer wird in SDR umgewandelt gespeichert, auch während er über die [HDR-Ausgabe](#hdr-ausgabe) angezeigt wird. Dateien heißen `<title ID>-<YYYYMMDD-HHMMSS>.png` und werden in ein Verzeichnis `anyps5-screenshots/` neben der ausgegebenen Programmdatei geschrieben, das bei Bedarf angelegt wird; setze `ANYPS5_SCREENSHOTS`, um ein anderes Verzeichnis zu verwenden. Der gespeicherte Pfad wird auf stderr ausgegeben. Tastendrücke, während der vorherige Screenshot noch geschrieben wird, werden zu einem zusammengefasst. Die Taste ist die Aktion `Screenshot` der [Eingabebelegung](INPUT_MAPPING.de.md). Ein Screenshot, der sich nicht schreiben lässt, beendet das Spiel mit dem Fehler.

### Lautstärke und Stummschaltung des Hosts

F5 senkt die Ausgabelautstärke des Hosts für das Spielfenster, F6 erhöht sie und F8 schaltet sie stumm; die Tasten lassen sich mit `VolumeDown`, `VolumeUp` und `ToggleMute` in [der Eingabebelegung](INPUT_MAPPING.de.md) ändern. Das ist eine Steuerung des Hosts, wie die Lautstärke eines Mediaplayers: Es ist kein Verhalten der Konsole, das Spiel sieht sie nicht, und seine eigenen Lautstärkeeinstellungen werden nicht geändert.

Die Lautstärke reicht von 0 bis 100 in Schritten von 5 und beginnt bei 100 oder bei `ANYPS5_VOLUME` (eine ganze Zahl von 0 bis 100). Die Ausgabeamplitude ist das Quadrat der Lautstärke als Anteil von 100, sodass 50 ein Viertel der Amplitude ergibt. Die Stummschaltung ist von der Lautstärke getrennt: Das Aufheben der Stummschaltung bringt die Lautstärke zurück, und eine Lautstärketaste hebt eine Stummschaltung auf. Eine Änderung der Verstärkung verläuft über 10 ms, statt zu springen. Jeder Tastendruck gibt auf stderr eine Zeile mit dem resultierenden Pegel oder der Stummschaltung aus; eine Anzeige auf dem Bildschirm gibt es nicht.

Bei Lautstärke 100 ohne Stummschaltung wird das Audio unverändert und ganz ohne Verarbeitung an das Host-Gerät weitergegeben. Die Lautstärke gilt für das, was das Spiel über `libSceAudioOut` und `libSceAudioOut2` abspielt, einschließlich dessen, was `libSceAudio3d` darüber abspielt. Die eigene Soundkarte und die Haptik des Controllers sowie das Mikrofon sind nicht betroffen.

Setze `ANYPS5_MUTE_ON_FOCUS_LOSS=1`, um das Audio stummzuschalten, solange das Anzeigefenster keinen Fokus hat, und es wiederherzustellen, wenn der Fokus zurückkehrt. `0` oder nicht gesetzt lässt das Audio unverändert. Die Einstellung ändert die mit der Taste gesetzte Stummschaltung nicht: Ein Fenster, das du stummgeschaltet hast, bleibt stumm, wenn es den Fokus zurückerhält. Nur Fokusereignisse zählen, sodass ein Fenster, das minimiert wird, ohne den Fokus zu verlieren, hörbar bleibt, und ein Fenster, das ohne Fokus öffnet, stumm startet. Ein ungültiger Wert einer der beiden Variablen, auch ein leerer, beendet das Spiel mit einem Fehler, der sie nennt.

Die Tasten funktionieren, solange das Anzeigefenster den Fokus hat. Nicht an einem echten Audiogerät oder an einer Konsole gemessen.

Setze `ANYPS5_AUDIO_IN_MUTE_ON_FOCUS_LOSS=1`, um dem Guest stille Mikrofon-Samples zu liefern, solange das Anzeigefenster keinen Fokus hat. Die Aufnahme bleibt in SDL geöffnet, aber Samples in der Warteschlange werden bei einem Fokuswechsel verworfen, damit Sprache, die während der Stummschaltung aufgenommen wurde, nach der Rückkehr des Fokus nicht ankommt. `0` oder nicht gesetzt lässt die Mikrofoneingabe unverändert; ein ungültiger Wert beendet das Spiel mit einem Fehler, der die Variable nennt.

### GPU-Auswahl

Das Spiel läuft auf dem ersten Gerät mit Vulkan 1.1, Grafik- und Compute-Queues und Swapchain-Präsentation, wobei eine dedizierte GPU einer integrierten vorgezogen wird. Setze `ANYPS5_GPU` auf einen Teil eines Gerätenamens, verglichen ohne Beachtung der Groß- und Kleinschreibung, um auf einem anderen Gerät zu laufen; die Namen werden beim Start in den Zeilen `Physical device candidate` ausgegeben. Wenn kein nutzbares Gerät den Text enthält, scheitert der Start, und der Fehler listet die Gerätenamen auf.

Das Fenster präsentiert mit vertikaler Synchronisation (`fifo`). Setze `ANYPS5_PRESENT_MODE` auf `fifo_latest_ready`, `mailbox` oder `immediate`, damit das Spiel nicht auf die Anzeige wartet: `fifo_latest_ready` und `mailbox` zeigen weiterhin ganze Frames im Takt der Bildwiederholrate und verwerfen die dazwischen, `immediate` präsentiert sofort und kann Tearing zeigen. Der Start scheitert bei jedem anderen Wert und wenn die Surface den Modus nicht anbietet (`fifo_latest_ready` braucht `VK_KHR_present_mode_fifo_latest_ready` oder dessen EXT-Form). Ein Wayland-Compositor kann aufhören, Frames an ein Fenster zu liefern, das er nicht zeigt, etwa eines auf einem anderen Workspace: Unter Hyprland mit dem NVIDIA-Treiber präsentiert eine `fifo`-Swapchain dann etwa einmal pro Sekunde, während `fifo_latest_ready` und `immediate` das Spiel bei seiner normalen Rate halten.

Setze `ANYPS5_FPS_LIMIT` auf eine Anzahl Frames pro Sekunde, von 0.5 bis 1000000 mit höchstens drei Nachkommastellen (`30`, `59.94`), um höchstens so viele zu präsentieren. Jeder Flip des Fensters wartet, bis er an der Reihe ist, bevor er präsentiert wird, und erst dann wird der Flip des Spiels abgeschlossen, sodass das Spiel wie von einer Anzeige mit dieser Rate getaktet wird; das Spiel erfährt davon nichts, und seine Vblank-Ereignisse und seine Bildwiederholrate bleiben die der Ausgabe. `0`, ein leerer Wert und eine nicht gesetzte Variable bedeuten kein Limit, das Verhalten ohne die Variable. Jeder andere Wert beendet das Spiel bei seinem ersten Flip mit einem Fehler, der die Variable nennt.

In jedem Present-Modus taktet die Ausgabe Flips standardmäßig mit 59.94 Hz oder im gleichnamigen Ausgabemodus mit 119.88 Hz. Setzt das Spiel eine Flip-Rate von 1 oder 2, warten Flips auf jedes zweite bzw. dritte Vblank. Das Limit kann diese Rate weiter senken. Ein Spiel, das bereits langsamer als das Limit ist, wird nicht verzögert. Ein Stocken wird nicht durch einen Schub nachgeholt: Ein Frame, der um weniger als eine Periode zu spät ist, wird durch den nächsten aufgeholt, und nach einem längeren Stocken wird höchstens ein Frame vorzeitig präsentiert. Das Warten schläft bis kurz vor seiner Frist und wartet den Rest aktiv ab. Fensterereignisse werden zwischen Flips verarbeitet, sodass das Fenster bei einem niedrigen Limit etwa einmal pro Frame reagiert. Mit `fifo` zeigt die Anzeige weiterhin bei jeder Bildwiederholung einen Frame, sodass ein Limit, das die Bildwiederholrate nicht teilt, manche Frames für eine Bildwiederholung und manche für zwei zeigt. Die Begrenzung ist eine Entscheidung des Hosts; die Konsole hat keine.

Unter Windows kann das OpenCL, OpenGL, and Vulkan Compatibility Pack Mesa Dozen installieren, das D3D12-Geräte über Vulkan bereitstellt. Manche D3D12-Treiber können abstürzen, während der Vulkan-Loader Adapter aufzählt, selbst wenn AnyPS5 ein natives Vulkan-Gerät wählen würde. Wenn das passiert und das System einen anderen Vulkan-Treiber hat, deaktiviere Dozen für die aktuelle PowerShell-Sitzung, bevor du AnyPS5 startest:

```powershell
$env:VK_LOADER_DRIVERS_DISABLE = '*dzn*'
.\app.exe
```

Deaktiviere Dozen nicht, wenn es der einzige verfügbare Vulkan-Treiber ist; die Anwendung hätte dann kein Gerät. `VK_LOADER_DRIVERS_DISABLE` wird mit den Dateinamen der Vulkan-Treibermanifeste abgeglichen und erfordert einen Vulkan-Loader, der mit Headern 1.3.234 oder neuer gebaut wurde ([Loader-Dokumentation](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderInterfaceArchitecture.md#filter-environment-variables)). Loader bis Version 1.4.365 ignorieren die Variable in einem Prozess mit erhöhten Rechten, etwa einem, der aus einer mit **Als Administrator ausführen** geöffneten PowerShell gestartet wurde.

Unter Windows kann der NVIDIA-Treiber 610.47 beim Erstellen von Pipelines in seinem Shader-Compiler abstürzen: Das Log endet mit `FATAL: unhandled exception 0xc0000005` mit `nvgpucomp64.dll` in `rip`, auf einem Treiber-Thread. PPSA25344 auf einer RTX 3060 endete so in 3 von 3 Ladevorgängen von „New Game“, auch mit `APS5_NO_PIPELINE_LTO=1`; mit Treiber 617.42 liefen derselbe Build und dieselbe Szene 40 Minuten lang. Aktualisiere den Treiber, wenn der Absturz in `nvgpucomp64.dll` liegt.

### Host-Bericht

Setze `ANYPS5_HOST_REPORT=1`, um einmal, sobald das Vulkan-Gerät des Spiels gewählt ist, auf stderr auszugeben, was ein Fehlerbericht braucht. Hänge den ganzen Block an das Issue an; er ersetzt die Angabe von OS, GPU und Treiber von Hand. `0`, ein leerer Wert oder keine Variable gibt nichts aus, und jeder andere Wert bricht den Start mit einem Fehler ab, der die Variable nennt.

Der Bericht hat feste Abschnitte in fester Reihenfolge, sodass zwei Läufe auf einem Rechner denselben Text ausgeben: `[system]` (Commit, OS (Kernel-Release und Maschinentyp unter Linux und macOS, Version und Build unter Windows), CPU-Modell, Hardware-Threads, physischer Arbeitsspeicher in MiB und die CPU-Prüfung), `[sdl]` (die zur Laufzeit gelinkte SDL-Version und die, gegen die gebaut wurde), `[gpu]` (Gerätename und -typ, PCI-Vendor- und Device-IDs, Vulkan-Version, Treibername und -info aus `VK_KHR_driver_properties`, die Treiberversion, dekodiert so, wie NVIDIAs Treiber, Intels Windows-Treiber oder die Versionsmakros von Vulkan sie kodieren (`unknown` ohne `VK_KHR_driver_properties` und unter MoltenVK), und ihr Rohwert, Subgroup-Größe, der größte Device-Local-Heap, jedes gefundene Vulkan-Gerät, die Present-Modi der Surface und der, den `ANYPS5_PRESENT_MODE` angefordert hat), `[vulkan]` (jede Erweiterung und jedes Feature, das das Spiel braucht, und die optionalen Erweiterungen, die es verwendet, markiert als `present` oder `missing`), `[shader_cache]` (`disabled`, `absent` oder `present` mit seiner Größe in MiB) und `[environment]` (die gesetzten `APS5_`-, `ANYPS5_`-, `VK_`- und `SDL_`-Variablen). Eine Angabe, die sich nicht lesen ließ, wird als `unavailable` ausgegeben und in einem abschließenden Abschnitt `[unavailable]` erklärt. Die Zeile `cpu_gate` sagt immer `passed`, wenn der Bericht erscheint, weil eine Host-CPU, der eine erforderliche Erweiterung fehlt, das Spiel beendet, bevor das Vulkan-Gerät gewählt wird (siehe die CPU-Anforderungen unter Eingabe und Konvertierung); sie sagt `unavailable`, wo diese Prüfung nicht läuft, etwa unter macOS. Die Größe des Shader-Caches zählt höchstens 200000 Dateien.

Der Bericht enthält keine Benutzernamen, Hostnamen, Seriennummern oder MAC-Adressen. Jedes Wort, das wie ein Pfad aussieht (beginnt mit `/` oder `~`, enthält einen Backslash oder beginnt mit einem Laufwerksbuchstaben), wird als `<path>` ausgegeben, URLs als `<url>`, MAC-Adressen als `<redacted>`, und Kontoname, Hostname und Name des Home-Verzeichnisses werden überall ersetzt, wo sie als ganzes Wort vorkommen. Prüfe den Text trotzdem vor dem Posten, da die Werte der aufgeführten Variablen so ausgegeben werden, wie sie gesetzt sind.

Wenn der gewählten GPU eine Erweiterung oder ein Feature fehlt, das das Spiel braucht (`VK_KHR_8bit_storage`, `VK_KHR_buffer_device_address`, `VK_KHR_shader_float_controls`, `storageBuffer8BitAccess`, `bufferDeviceAddress`, `shaderInt64`, `shaderSignedZeroInfNanPreserveFloat32`, `vertexPipelineStoresAndAtomics`, `fragmentStoresAndAtomics`, `samplerAnisotropy`, `textureCompressionBC` und `VK_KHR_swapchain` für ein Fenster), scheitert der Start mit einer Meldung, die die GPU, ihren Treiber und ihre Vulkan-Version nennt, jedes fehlende Element auflistet und auf die anderen Vulkan-Geräte verweist, die `ANYPS5_GPU` wählen kann. Mit `ANYPS5_HOST_REPORT=1` wird der Bericht zuerst ausgegeben, sodass auch ein gescheiterter Start ihn hat.

### HDR-Ausgabe

HDR-Ausgabe gibt es nur unter Windows. Spiele sehen sie, wenn der Monitor, der das Fenster zeigt, im HDR-Modus läuft (Einstellungen > Anzeige > HDR verwenden), die Vulkan-Surface des Fensters eine HDR10-Swapchain (`VK_COLOR_SPACE_HDR10_ST2084_EXT`) in einem 10-Bit-Format anbietet, die Instanz `VK_EXT_swapchain_colorspace` und das Gerät `VK_EXT_hdr_metadata` hat. Die Leuchtdichte des Monitors kommt von DXGI; ein Monitor, der keine Spitzenleuchtdichte meldet, wird mit 1000 Nits für Spitze und Vollbild und 0 Nits Schwarz angenommen. Unter Linux und auf anderen Hosts ist die Ausgabe immer SDR, weil die Formate der Surface nicht zeigen, ob die Anzeige im HDR-Modus ist. Die BT.2100-PQ-Anzeigepuffer eines Spiels erreichen die Anzeige dann unverändert; ohne HDR-Ausgabe werden sie in SDR umgewandelt.

### Shader-Cache

Rekompilierte Shader und der Vulkan-Pipeline-Cache werden in einem Verzeichnis `shader_cache/` neben der ausgegebenen Programmdatei abgelegt, sodass spätere Läufe das Rekompilieren überspringen. Setze `ANYPS5_SHADER_CACHE_DIR`, um ein anderes Verzeichnis zu verwenden, oder setze `ANYPS5_NO_SHADER_CACHE` auf einen nicht leeren Wert außer `0`, um beide Caches zu deaktivieren. Shader werden in einem Unterverzeichnis gespeichert, das nach der Quellversion des Recompilers benannt ist; Unterverzeichnisse anderer Versionen, die 14 Tage lang nicht verwendet wurden, werden entfernt, wenn ein Lauf zum ersten Mal einen Shader nachschlägt. Der Pipeline-Cache ist eine Datei `vk-pipelines-*.bin` pro GPU und Treiberversion im selben Unterverzeichnis, sodass er bei den Shadern liegt, aus denen er gebaut wurde. Dasselbe Unterverzeichnis enthält auch jeden Shader in der Spezialisierung für die Ressourcen und die GPU, mit denen er zuerst verwendet wurde, sodass ein späterer Lauf ihn beim Aufzeichnen des Draws oder Dispatchs nicht erneut spezialisiert; `APS5_NO_SPECIALIZED_CACHE=1` spezialisiert stattdessen jeden Shader erneut. Die Zeile `[shader-disk-cache]` zählt diese Einträge als `specialized modules`. Einträge der aktuellen Version werden nie entfernt, und das Verzeichnis hat keine Größenbeschränkung, sodass ein Spiel, das viele Shader kompiliert, innerhalb von Minuten mehrere GB hinzufügen kann. Das Verzeichnis zu löschen ist unbedenklich: Es wird beim nächsten Lauf neu aufgebaut.

Ein erster Lauf kann bei einem sehr großen Compute-Shader stehen bleiben, während der GPU-Treiber dessen Pipeline kompiliert: Es werden keine neuen Frames angezeigt, und der Compiler des Treibers hält mehrere CPU-Threads beschäftigt, was bei Shadern mit mehreren Millionen SPIR-V-Wörtern Minuten bis Stunden dauern kann. Mit `APS5_PROFILE_DRAW=1` nennt das Log eine solche Kompilierung, bevor sie beginnt (`[dispatch] creating a pipeline for N SPIR-V words`, ausgegeben ab mehr als 100000 Wörtern), und ihre Dauer, wenn sie endet (`[dispatch] created a pipeline for N SPIR-V words in N ms`). Der committete Speicher des Spiels kann während der Kompilierung weiter wachsen, daher braucht das Commit-Limit des Systems (siehe Aufbau zur Laufzeit) Platz dafür; vergrößere unter Windows vor einem solchen Lauf die Auslagerungsdatei. Spätere Läufe können den Pipeline-Cache des Treibers wiederverwenden, und das Pipeline-Journal baut aufgezeichnete Pipelines in Hintergrund-Threads neu, was das Warten vermeiden kann, wenn sie zum ersten Mal erreicht werden; keines von beiden garantiert, dass der Treiber die Kompilierung überspringt, und ein Treiber-Update oder eine andere GPU beginnt einen eigenen Cache.

### Pipeline-Journal

Neben den rekompilierten Shadern enthält das Shader-Cache-Verzeichnis `pipelines.journal`: jede Vulkan-Grafik-Pipeline, die der Pipeline-Store behalten hat, und jede Compute-Pipeline, die ein Lauf gebaut hat, mit den spezialisierten Shader-Modulen, aus denen sie gebaut wurden. Der nächste Lauf des Spiels baut diese Pipelines während des Ladens in Hintergrund-Threads, sodass ein Draw oder Dispatch, den der frühere Lauf schon ausgeführt hat, seine Pipeline nicht kompiliert, wenn er zum ersten Mal erreicht wird. Das Journal gehört zu der GPU und dem Treiber, die es geschrieben haben, wie die Pipeline-Cache-Datei des Treibers daneben: Ein Lauf auf einer anderen GPU oder Treiberversion beginnt es neu, und der eigene Pipeline-Cache des Treibers entscheidet, wie schnell jede Pipeline neu gebaut wird. Zwei Läufe, die sich das Verzeichnis teilen, wechseln sich über `pipelines.journal.lock` ab; ein Lauf, der es nicht sperren kann, zeichnet nichts auf. `APS5_NO_PIPELINE_JOURNAL=1` deaktiviert Aufzeichnung und Wiedergabe; `APS5_PIPELINE_JOURNAL_THREADS` setzt die Wiedergabe-Threads (standardmäßig die Hälfte der CPU-Threads; `0` zeichnet auf, ohne wiederzugeben), `APS5_PIPELINE_JOURNAL_ENTRIES`, wie viele Pipelines wiedergegeben werden (4096, die zuletzt aufgezeichneten), `APS5_PIPELINE_STORE_ENTRIES`, wie viele Grafik-Pipelines resident bleiben (standardmäßig wächst der Store von 1024 bis 4096, um die Wiedergabe und die immer wieder benötigten Pipelines zu halten), und `APS5_PIPELINE_JOURNAL_MAX_MIB` (1024) die Dateigröße, ab der die Aufzeichnung stoppt. Das Löschen der Datei kostet nur den Vorsprung. Mit `APS5_PROFILE_DRAW=1` meldet eine `[pipeline-journal]`-Zeile, was geladen, wiedergegeben und aufgezeichnet wurde.

### Draw-Pipeline

`APS5_DRAW_PIPELINE` (aus, wenn nicht gesetzt; jeder Wert, auch `0`, schaltet sie ein, üblich ist `=1`) verlagert mehr von jedem Draw weg von dem Thread, der die Command-Buffer des Spiels liest: Ein zweiter Thread schließt jeden Draw ab, während der nächste gelesen wird, der Thread, der Draws aufzeichnet, überspringt Arbeit, deren Eingaben sich nicht geändert haben, und Prüfungen, ob die Ressourcen eines Draws noch aktuell sind, werden wiederverwendet, bis das Spiel oder die GPU Speicher schreibt. Gedacht ist sie für den Einsatz mit `APS5_PIPELINED_DRAWS=32 APS5_PIPELINE_DISPATCH=1`. Das Bild ändert sich nicht; wenn doch, ist das ein Bug, und ein Lauf ohne den Schalter zeigt, ob er die Ursache ist. `APS5_VERIFY_DRAW_PIPELINE=1` führt die vollständigen Prüfungen neben den wiederverwendeten aus, auf Kosten der Geschwindigkeit, und gibt `[epoch-proofs]`-Zeilen mit der Anzahl der Abweichungen aus (jede Anzahl ungleich null ist ein Bug). `APS5_PROFILE_DRAW=1` schließt jeden Draw auf dem lesenden Thread ab, sodass ein Lauf damit diesen Schalter nicht misst. Um die Übergabe ohne das zu sehen, setze `APS5_PIPELINE_STATS` (beliebiger Wert; `1` schaltet zusätzlich Vulkan-Pipeline-Statistiken ein, die Zeit kosten): Alle 10 s zählt eine `[finisher]`-Zeile die an den zweiten Thread übergebenen Draws und die auf dem lesenden Thread abgeschlossenen, und `[finisher-held]`-Zeilen zählen die zurückgehaltenen Draws, aufgeschlüsselt nach dem Grund, warum ihre Schreibzugriffe nicht bekannt sind.

### Budget des Textur-Caches

Speicherallokationen für Images und Buffer werden wiederholt, nachdem ungenutzte Buffer aus dem Pool und freigebbare Einträge des Textur-Caches freigegeben wurden. Die Wiederherstellung bewahrt Storage-Images mit ausstehenden GPU-Ergebnissen und Ressourcen, die von aufgezeichneter Arbeit gehalten werden. `[bufferpool]`-Zeilen zu Wiederherstellung und Fehlschlag melden die angeforderte Allokation, den freigegebenen Speicher und die Anzahl der Wiederholungen. Eine Allokation scheitert weiterhin, wenn die Wiederherstellung nicht genug Speicher freigeben kann. Wenn das Budget von `VK_EXT_memory_budget` für den Device-Local-Heap keinen Platz für ein Image, einen Device-Buffer oder einen residenten Heap-Mirror-Slab hat, geben die Textur-Caches zuerst freigebbare Einträge im Umfang der Allokation und des Betrags frei, um den der Heap sein Budget überschreitet; die Allokation kommt nur dann in den Host-Speicher, wenn sie weniger als das freigeben und das erneut gelesene Budget immer noch keinen Platz hat.

Gesampelte Texturen und Storage-Images bleiben im Gerätespeicher in zwei Caches, die ihre am längsten nicht verwendeten Einträge verdrängen, wenn sie über ihr Budget wachsen. Das Budget für Storage-Images ist das, was das Budget von `VK_EXT_memory_budget` für den größten Device-Local-Heap nach Abzug der Nutzung des Heaps ohne die gecachten Storage-Images selbst und eines Achtels des Budgets übrig lässt; es ist auf die Hälfte dieses Budgets oder die Hälfte der Heap-Größe begrenzt, je nachdem, was kleiner ist, und nie kleiner als 512 MiB. Das Budget für gesampelte Texturen ist das, was das gemeldete Budget nach Abzug der Nutzung des Heaps ohne gecachte Texturen, eines Viertels der Heap-Größe (mindestens 2 GiB) und eines Achtels des Budgets übrig lässt, und nie kleiner als 2 GiB. Wenn die übrige Nutzung des Heaps wächst, sinkt zuerst das Budget für gesampelte Texturen bis zu dieser Untergrenze, dann das für Storage-Images; das Storage-Budget gibt außerdem ab, was die lebenden gesampelten Texturen beim Wachsen belegen. Jeder Cache kürzt bei seinem nächsten Einfügen. Beide Budgets werden jede Sekunde neu gelesen; `[gpu] sampled texture cache budget`-Zeilen melden sie beim Start und wenn sich eines um mehr als ein Zehntel ändert, höchstens alle 10 s. `APS5_TEXTURE_CACHE_MIB=<n>` legt beide Budgets auf `n` MiB fest, und die Zeilen werden dann nicht ausgegeben.

Ein festes Budget über dem automatischen kann mehr sein, als die Karte neben dem Rest des Frames fasst. Allokationen gehen dann in den Host-Speicher (`[gpu] ... placed in host memory`-Zeilen), wo die GPU sie über PCIe liest. PPSA16064 auf einer RTX 4060 8 GB, deren automatisches Budget für Storage-Images nach der früheren Regel 1797 MiB betrug, legte mit `APS5_TEXTURE_CACHE_MIB=3072` 868 und 878 MiB in den Host-Speicher und durchlief sein erstes Level mit 4.46 und 4.83 FPS; ohne die Variable legte es nichts dort ab und durchlief denselben Weg mit 6.48 und 6.07 FPS. Lass sie ungesetzt, sofern keine Messung auf der Karte etwas anderes zeigt.

### GPU-Ergebnisse bei jedem Flip speichern

GPU-Ergebnisse in Storage-Images und Render-Targets bleiben auf der GPU, bis etwas ihren Speicher liest. Bei einem Spiel, das ein solches Image freigibt, nachdem es auf seine GPU-Arbeit gewartet hat, und den Speicher wiederverwendet, können die neuen Daten überschrieben werden, wenn die Ergebnisse später gespeichert werden ([technische Schulden](../dev/debt/libSceAgcDriver.md)). `APS5_STORE_AT_FLIP=1` speichert jedes ausstehende Ergebnis, wenn ein Anzeigepuffer geflippt wird, auf Kosten eines Write-backs pro ausstehendem Image und Frame. Jeder andere Wert beendet das Spiel bei seinem ersten Flip.

Eigene CPU-Lesezugriffe des Spiels auf diesen Speicher werden nicht erkannt, sodass ein Spiel, das auf ein GPU-Label wartet und dann die Ergebnisse liest, veralteten Speicher liest. `APS5_STORE_AT_LABEL=1` speichert jedes ausstehende Ergebnis vor jedem Label, das die GPU schreibt (`RELEASE_MEM`, `WRITE_DATA`, `DMA_DATA`), auf Kosten eines Write-backs jedes ausstehenden Images pro Label. Jeder andere Wert beendet das Spiel bei seinem ersten Label.

### Hochskalierung

Das Fenster zeigt den Anzeigepuffer des Spiels mit bilinearer Filterung passend skaliert. Setze `ANYPS5_UPSCALE=fsr`, um ihn stattdessen mit AMD FidelityFX Super Resolution 1.0 zu skalieren. Wenn das eingepasste Bild auf einer der Achsen größer ist als der Anzeigepuffer, etwa ein 1920x1080-Anzeigepuffer in einem 2560x1440- oder 3840x2160-Fenster, skaliert EASU es hoch, und RCAS schärft das Ergebnis. EASU ist für bis zum Doppelten der Größe des Anzeigepuffers auf jeder Achse ausgelegt: Größere Faktoren, etwa ein 1280x720-Puffer in einem 3840x2160-Fenster, funktionieren weiterhin, sehen aber weicher aus. Ist das eingepasste Bild gleich groß oder kleiner, bleibt FSR für diesen Frame aus, und er wird genau wie ohne `ANYPS5_UPSCALE` präsentiert: RCAS allein würde dort Bildrate kosten, ohne Details hinzuzufügen.

`ANYPS5_FSR_SHARPNESS` setzt die RCAS-Schärfe in Stops, von `0.0` (am schärfsten) bis `2.0` (am schwächsten); Standard ist `0.2`. Ohne `ANYPS5_UPSCALE=fsr` hat es keine Wirkung. RCAS läuft nur auf Frames, die FSR hochskaliert, und dort schärft es immer: `2.0` ist seine schwächste Einstellung, nicht aus. Sein angenäherter Kehrwert dunkelt außerdem flache Bereiche um einen festen Anteil ihres Werts ab: 0.17% beim Standard `0.2` (ein flaches 0.5 wird zu 0.4992), 0.32% bei `0.0` und 0.07% bei `2.0`. Das ist höchstens eine 8-Bit-Stufe bei SDR und höchstens drei 10-Bit-Codewerte bei HDR10.

FSR arbeitet bei SDR- und HDR10-Ausgabe gleichermaßen auf den Werten, die die Anzeige erhält: sRGB-kodiert bei SDR, PQ-Codewerte bei HDR10. Das Schärfen bringt sie nie aus dem Signalbereich. Während EASU läuft, hält jede laufende Präsentation zwei weitere Images, eines in der Größe des Anzeigepuffers und eines in der Größe des eingepassten Bildes, und jeder Frame braucht zwei Durchläufe über das ganze Fenster.

Ein leerer Wert einer der beiden Variablen gilt wie ein nicht gesetzter. Der Start scheitert bei jedem anderen Wert von `ANYPS5_UPSCALE` als `fsr` und bei einem `ANYPS5_FSR_SHARPNESS`, das keine Zahl von 0.0 bis 2.0 ist.

### Benchmark-Berichte

Um zu messen, wie ein Spiel auf einem Rechner läuft, starte es mit `APS5_TRACE_SUMMARY_EVERY=30 APS5_TRACE_BENCH=1`, spiele die Szene einige Minuten und beende es. Alle 30 Sekunden schreibt die Laufzeitumgebung `aps5-bench-<commit>-<pid>.json.gz` in `APS5_TRACE_DIR` oder im Arbeitsverzeichnis neu und hängt eine Zeile an `aps5-summary.jsonl` an. Die Datei enthält die Zeit zwischen je zwei Presents, den Commit, GPU, Treiber, OS, CPU-Namen, Thread-Anzahl und physischen Arbeitsspeicher sowie, wenn `app0/sce_sys/param.json` vorhanden ist, Titel-ID, Name und Inhaltsversion. Sie enthält keine Spieldaten. Gespeichert werden nur abgeschlossene Intervalle, die letzten 240; `intervals_dropped` zählt ältere Intervalle, die aus der Datei entfernt wurden. Das Intervall, das beim Beenden des Spiels noch offen ist, wird nicht geschrieben. Die Felder sind unter [Zusammenfassungen für eine lange Sitzung](../dev/BUILD.md#summaries-for-a-long-session) beschrieben.

Lies eine Aufzeichnung mit Python 3 aus dem Quell-Checkout:

```sh
python3 tools/benchmark_compare.py summarize captures/run.json.gz
python3 tools/benchmark_compare.py summarize captures/run.json.gz --intervals 1:2 --json
```

`--intervals START:COUNT` wählt ganze abgeschlossene Intervalle nach ihrer nullbasierten Position in der gespeicherten Datei: `1:2` wählt die Positionen 1 und 2. Ohne die Option werden alle gespeicherten Intervalle einbezogen. Ein Intervall, das während des Ladens oder der Shader-Kompilierung aufgezeichnet wurde, bleibt einbezogen, sofern du keinen späteren Bereich wählst. Der Bericht zeigt die aufgezeichneten Zeiten und globalen Present-Nummern der gewählten Intervalle, auch wenn frühere Verlaufsdaten verworfen wurden. Ein gewählter Bereich, der eine Lücke überspannt, wird abgelehnt.

FPS ist die Anzahl der Frames geteilt durch die Summe der verstrichenen Zeiten der gewählten Intervalle. Mittlere Frame-Zeit sowie p50-, p90-, p99- und maximale Frame-Zeit fassen die aufgezeichneten Frame-Dauern zusammen, die auf Mikrosekunden abgeschnitten sind. Diese Werte können vom Kehrwert der FPS abweichen. Gesättigte Dauern erzeugen eine Warnung und machen die betroffenen Metriken auf Basis der Rohdauern nicht verfügbar.

Für wiederholte A/B-Läufe schreibe eine Serien-JSON neben die Aufzeichnungen. Führe die Versuche in der Reihenfolge auf, in der sie liefen, mit `baseline` und `candidate` als Arme. Gib jedem beabsichtigten Paar denselben `pair`-Wert. Dieses Beispiel zeichnet A, B, B, A auf, danach zwei erfolglose Versuche:

```json
{
  "format": "aps5-bench-series/1",
  "recipe": {
    "scene": "Load the same save and stand still after loading",
    "save": "Local save slot 1",
    "input": "No input during the selected intervals",
    "cache": "Warm caches"
  },
  "runs": [
    {"id": "A1", "arm": "baseline", "pair": "1", "capture": "captures/base-1.json.gz", "intervals": [1, 2]},
    {"id": "B1", "arm": "candidate", "pair": "1", "capture": "captures/change-1.json.gz", "intervals": [1, 2]},
    {"id": "B2", "arm": "candidate", "pair": "2", "capture": "captures/change-2.json.gz", "intervals": [1, 2]},
    {"id": "A2", "arm": "baseline", "pair": "2", "capture": "captures/base-2.json.gz", "intervals": [1, 2]},
    {"id": "B3", "arm": "candidate", "pair": "3", "status": "failed", "reason": "No benchmark was produced"},
    {"id": "A3", "arm": "baseline", "pair": "3", "status": "excluded", "reason": "The display slept during the run"}
  ]
}
```

Aufzeichnungspfade sind relativ zur Seriendatei. `intervals` ist `[START, COUNT]` und kann weggelassen werden, um alle gespeicherten Intervalle einzubeziehen. Ein einbezogener Lauf kann `sha256` angeben, um zu verlangen, dass die ursprünglichen Bytes der Aufzeichnung diesem Digest entsprechen. Fehlgeschlagene und ausgeschlossene Versuche brauchen einen Grund und haben keine Aufzeichnung. Behalte sie in der Serie, damit der Bericht jeden angegebenen Versuch zeigt. Die Wiederverwendung derselben Aufzeichnung, auch einer umbenannten oder neu komprimierten Kopie, wird als doppelter Nachweis abgelehnt.

```sh
python3 tools/benchmark_compare.py compare series.json
python3 tools/benchmark_compare.py compare series.json --require-matching-context --json
```

Der Vergleich erhält die Reihenfolge der Läufe, meldet jedes Paar und zählt vollständige Paare gesondert. Änderungen sind Kandidat minus Basis; die Gesamtauswertung meldet Mittelwert und Spannweite der gepaarten prozentualen Änderungen. Ein Paar mit einem fehlgeschlagenen, ausgeschlossenen oder fehlenden Arm trägt keine gepaarte Änderung bei.

Aufgezeichnete Kontextunterschiede, Dirty Builds und fehlende Identitätsfelder erscheinen im Bericht. `--require-matching-context` lehnt aufgezeichnete Kontextunterschiede ab. Das Rezept hält die Angaben des Durchführenden zu Szene, Spielstand, Eingabe und Caches fest. Prüfe, dass die Läufe dieselbe Szene erreicht haben: Übereinstimmende Metadaten und Hashes beweisen weder unabhängige Läufe noch, dass eine Codeänderung einen Geschwindigkeitsunterschied verursacht hat. Das Tool liest vorhandene Dateien und gibt Markdown aus, mit `--json` JSON.

### Laufbericht

Jeder Lauf eines konvertierten Spiels schreibt `anyps5-report.json`, wenn er über `exit` endet: welches Spiel es war, welcher Build es ausgeführt hat, wie weit es kam, wie viele Frames präsentiert wurden, Zahlen zu Shadern und fehlenden Funktionen. Der Bericht bleibt auf deinem Rechner; nichts wird hochgeladen. Die Felder sind unter [Laufbericht](run-report.de.md) beschrieben; `docs/run-report.schema.json` ist das genaue Format.

Der Bericht ist standardmäßig an. `--aps5-report=off` in der Startbefehlszeile oder `APS5_REPORT=0` in der Umgebung schaltet ihn ab, und `--aps5-report=on` oder `APS5_REPORT=1` schaltet ihn für einen Prozess ein, der kein konvertiertes Spiel ist; das Flag hat Vorrang vor der Variable. Das Spiel sieht das Flag in seinen eigenen Argumenten. Bei abgeschaltetem Bericht wird nichts gesammelt und nichts geschrieben.

Der Bericht wird neben die Spielprogrammdatei geschrieben. Wenn dieser Ordner nicht beschreibbar ist, landet er in `reports/<run id>/anyps5-report.json` im Datenordner (`$XDG_DATA_HOME/AnyPS5` oder `~/.local/share/AnyPS5` unter Linux, `%LOCALAPPDATA%\AnyPS5` unter Windows, `~/Library/Application Support/AnyPS5` unter macOS). Unter Windows schreiben nur `exit` und `_exit` des Guests einen Bericht; eine Rückkehr aus dem Einstiegspunkt schreibt keinen. Ein Lauf, der an einer Exception endet, die nichts abfängt (`std::terminate`), etwa an einem Aufruf einer nicht implementierten Funktion, schreibt den Bericht mit Exit `abort`, bevor der Terminate-Handler läuft, oder danach, wenn das Spiel einen eigenen Handler installiert hat. Ein Lauf, der zwangsweise beendet wird oder abstürzt, schreibt keinen Bericht.

Welche Meilensteine ein Lauf aufzeichnet, hängt vom System ab:

| System | `loaded`, `started` | `firstDraw`, `firstPresent` | `stage`, Frames, Bildrate |
|---|---|---|---|
| Linux | nicht aufgezeichnet | aufgezeichnet | aufgezeichnet |
| Windows | nicht aufgezeichnet | aufgezeichnet | aufgezeichnet |
| macOS | aufgezeichnet | aufgezeichnet | aufgezeichnet |

Ein Meilenstein, der im Bericht fehlt, wurde entweder vom Lauf nicht erreicht oder wird von diesem System nicht aufgezeichnet: Unter Linux und Windows werden `loaded` und `started` nie aufgezeichnet, ihr Fehlen sagt also nichts aus, und `stage` und `framesPresented` sagen, wie weit der Lauf nachweislich gekommen ist. Der Code für Windows und macOS wurde nicht ausgeführt.

Um einen Lauf zu teilen, hänge `anyps5-report.json` an die Diskussion des Spiels an. Die Laufzeitumgebung filtert Benutzernamen, Home-Ordner, Hostnamen, Adressen und Kontonummern aus dem Bericht und schreibt nie Spielstände hinein. Das ist ein Filter, keine Garantie, also lies die Datei, bevor du sie teilst.

### Aufrufe von Systembibliotheken verfolgen

Windows, Linux und macOS. Eine mit `--trace-imports` konvertierte Programmdatei zeichnet die Aufrufe auf, die sie an die in `APS5_TRACE_CALLS` genannten Bibliotheken richtet, eine kommagetrennte Liste wie `libkernel,libSceSaveData`, oder `all`. Ohne die Variable wird nichts aufgezeichnet. Hat ein Import kein Bibliotheks-Label, verwendet der Tracer den Dateinamen des Host-Moduls, das seine Funktion enthält; wähle diesen Namen, etwa `libSceAvPlayer.native`, statt eines Guest-Alias. Die Absturz- und Abort-Berichte enden dann mit den letzten 64 Aufrufen (unter macOS, das keinen Absturzbericht hat, gibt nur ein Abort sie aus): Sequenznummer, Thread, Bibliothek, Importname, wie ihn die Programmdatei importiert (eine NID), und die Adresse des Aufrufers. Es folgen die letzten 32 dieser Aufrufe des meldenden Threads, entnommen aus den 65536 jüngsten Aufrufen, sodass ein Thread, dessen Aufrufe ein geschäftigerer aus den letzten 64 verdrängt hat, sie trotzdem zeigt. Dann folgen die 32 am häufigsten aufgerufenen Importe seit dem Start mit der Anzahl ihrer Aufrufe.

```
last traced system library calls (APS5_TRACE_CALLS):
  #1 thread 9384 libkernel.prx k+AXqu2-eBc from 0x0000000000400006 app.exe+0x10006
last traced calls of thread 9384:
  #1 libkernel.prx k+AXqu2-eBc from 0x0000000000400006 app.exe+0x10006
most called traced imports (1 calls in total):
             1 libkernel.prx k+AXqu2-eBc
```

Um jeden Import aufzulisten, den ein Spiel während eines Laufs ohne Absturz aufruft, setze außerdem `APS5_TRACE_CALLS_FILE` auf einen Dateipfad. Alle 10 Sekunden wird die Datei neu geschrieben, mit einer Zeile pro bisher aufgerufenem Import, der häufigste zuerst: die Anzahl der Aufrufe, die Bibliothek und der Importname. Sie übersteht einen zwangsweise beendeten Lauf bis zu ihrem letzten Neuschreiben.

```
245557 libc.prx gQX+4GDQjpM#guest
3866 libSceAgc.prx 57labkp+rSQ
```

Grenzen sind in den technischen Schulden zu [libc](../dev/debt/libc.md) und [Relinker](../dev/debt/relinker.md) aufgeführt.

### Leistungs-Overlay

F10 schaltet ein Leistungs-Overlay über dem dargestellten Bild durch drei Zustände: aus, Seite 1 (die Tabelle unten), Seite 2 (eine Detailseite, die zweite Tabelle), dann wieder aus. `ANYPS5_OVERLAY=1` zeigt Seite 1 ab dem ersten Frame und `ANYPS5_OVERLAY=2` Seite 2; F10 schaltet dann von dort aus weiter. `ANYPS5_OVERLAY=0` gilt wie nicht gesetzt. `ANYPS5_OVERLAY_POS` setzt es in eine Ecke des Fensters: `tl` (oben links, Standard), `tr`, `bl` oder `br`. Für beide Variablen gilt ein leerer Wert wie ein nicht gesetzter, und jeder andere Wert beendet das Spiel bei seinem ersten Frame mit einem Fehler, der die Variable nennt. Die Taste lässt sich mit `TogglePerformanceOverlay` in der [Eingabebelegung](INPUT_MAPPING.de.md) verlegen. Der Text skaliert mit der Fensterhöhe, in ganzzahligen Vielfachen seiner 5x7-Pixel-Schrift; auf einer HDR10-Swapchain wird das Overlay mit 203 Nits gezeichnet. `ANYPS5_OVERLAY_SCALE=N` (1 bis 8) fordert eine Textskalierung N an; das Overlay schrumpft trotzdem, um ins Fenster zu passen.

| Zeile | Bedeutung |
|------|---------|
| `PROFILED` | Die erste Zeile, solange `APS5_PROFILE_GPU=1` oder `APS5_PROFILE_DRAW=1` gesetzt ist. Diese Schalter fügen jedem Batch Timestamp-Queries und jedem Paket Timer hinzu, sodass die Zahlen eines solchen Laufs nicht mit denen eines Laufs ohne Profiling vergleichbar sind. |
| `FPS` | Präsentierte Frames pro Sekunde, über die letzte Sekunde. Wenn die letzte Sekunde weniger als zwei Presents enthält (unter 1 fps), stattdessen die Rate des letzten Frames, 1000 geteilt durch seine Zeit in ms: Ein Frame alle 2 s zeigt `0.50`. Jeder Present zählt, auch Frames, die ein Spiel leer präsentiert, etwa während es lädt. |
| `FRAME` | Zeit zwischen den letzten beiden präsentierten Frames, in ms. |
| Graph | Die letzten 120 Frame-Zeiten, die neueste rechts. Die volle Höhe ist die längste davon oder das Doppelte ihres Durchschnitts, je nachdem, was größer ist. |
| `MIN/AVG/MAX` | Die kürzeste, die durchschnittliche und die längste der 120 Frame-Zeiten, in ms. |
| `GPU` | GPU-Zeit pro Frame über die letzte Sekunde, oder die des letzten Frames unter 1 fps, in ms: die Spanne jedes aufgezeichneten Batches von seinem ersten bis zu seinem letzten Befehl, aus den Timestamp-Queries von `APS5_PROFILE_GPU=1`. Nur mit dieser Variable angezeigt. Batches werden gezählt, wenn sie abgeschlossen sind, sodass die Zeile einer plötzlichen Änderung um bis zu eine Sekunde nachläuft, unter 1 fps um einen Frame. Batches, die sich auf verschiedenen Queues überlappen, zählen jeweils voll, sodass die Zeile `FRAME` übersteigen kann. |
| `DRIVER` | CPU-Zeit pro Frame über die letzte Sekunde, oder die des letzten Frames unter 1 fps, in ms, die die Paket-Worker in Dispatch- und Draw-Paketen verbracht haben (die Zeit `dispatch` und `draw` der Zeile `[gpu] worker` von `APS5_PROFILE_DRAW=1`). Nur mit dieser Variable angezeigt. Die Zeit jedes Workers zählt voll, sodass die Zeile bei mehreren gleichzeitig beschäftigten Workern `FRAME` übersteigen kann. |
| `DRAWS` | Draw-Pakete pro Frame über die letzte Sekunde, oder die des letzten Frames unter 1 fps, dann Dispatch-Pakete pro Frame (`DISPATCH`), wie die Paket-Worker sie ausführen; Pakete, die die Prädikation überspringt, werden nicht gezählt. Angezeigt, sobald das Spiel eines von beiden übermittelt hat. |
| `SHADERS` | In der letzten Sekunde erstellte Pipelines, oder die während des letzten Frames unter 1 fps, dann seit dem Start (`TOTAL`): eine Grafik-Pipeline für jeden Draw-Zustand, den der Pipeline-Cache noch nicht gesehen hatte, und eine Compute-Pipeline für jeden neuen Dispatch-Shader. Ein Anstieg, während das Spiel ruckelt, deutet auf Pipeline-Kompilierung hin. Angezeigt, sobald eine Pipeline erstellt wurde. |
| `VRAM` | Nutzung und Budget des größten Device-Local-Speicher-Heaps der GPU aus `VK_EXT_memory_budget`, einmal pro Sekunde gelesen. Nicht angezeigt, wenn dem Treiber die Erweiterung fehlt. |
| `CPU` | Temperatur des CPU-Package in Grad Celsius, einmal pro Sekunde aus den sysfs-Sensoren für thermal und hwmon unter Linux gelesen (`/sys/class`). Nicht angezeigt, wenn kein Sensor eine meldet, und unter Windows nicht gelesen. |
| `RAM` | Der private Speicher des Prozesses, einmal pro Sekunde gelesen: `PrivateUsage` von `GetProcessMemoryInfo` unter Windows, `RssAnon` plus `VmSwap` aus `/proc/self/status` unter Linux. Der Direktspeicher des Spiels ist auf beiden Systemen gemeinsam genutzter Speicher und nicht enthalten. |
| `RES` | Die Größe des Anzeigepuffers, dann die Größe der Swapchain des Fensters, zum Beispiel `RES: 1920X1080 / 2560X1440`. Nicht angezeigt für Frames ohne Anzeigepuffer. |

Zeiten werden unter 100 mit einer Nachkommastelle und darüber als ganze Zahlen angezeigt; alles über 9999 wird als 9999 angezeigt, sodass `FRAME` und `MIN/AVG/MAX` bei Frames von 10 s oder mehr 9999 zeigen. `FPS` hat dasselbe Format, mit zwei Nachkommastellen unter 1. Zähler sind ganze Zahlen, ab 10000 in Tausend (`12K`) und ab einer Million in Millionen (`3M`). Speicher wird in GB zu 2^30 Bytes angegeben. Während das Overlay ausgeblendet ist, zeichnet die Präsentation nichts dafür auf, und das dargestellte Bild bleibt unverändert; während es angezeigt wird, zeichnet ein kleiner Render-Pass es über jeden präsentierten Frame. Jedes Draw- und Dispatch-Paket und jede Pipeline-Erstellung wird auch bei ausgeblendetem Overlay gezählt, mit einem Inkrement eines Zählers, der dem Paket-Worker gehört, bei Pipelines eines gemeinsamen Zählers; Speicher und Zähler werden nur gelesen, während das Overlay angezeigt wird.

Seite 2 ist breiter, 46 Zeichen, und hat keinen Graphen. Sie liest ihre Quellen einmal pro Sekunde, während sie angezeigt wird, sodass ihre Raten die letzte Sekunde abdecken; sie erscheinen eine Sekunde nach der Seite. Eine Zeile, deren Quelle im Lauf nicht verfügbar ist, wird weggelassen, ebenso ein Teil einer Zeile.

| Zeile | Bedeutung |
|------|---------|
| `FPS`, `FRAME` | Wie auf Seite 1. |
| `BUSY %` | Der Anteil der letzten Sekunde, den der Paket-Worker jeder Queue (`Q0` für Queue 0, `Q1` für Queue 1 usw.) mit Paketen, Suspends, Enden von Submissions, Rewinds und Abschlüssen verbracht hat, wie ihn `worker_busy_pct` der Trace-Zusammenfassung zählt. Aus den Trace-Summen. |
| Phasen | Die 5 Phasen mit der meisten CPU-Zeit pro Frame über die letzte Sekunde, in ms, summiert über die Threads des Treibers und benannt wie in `top_phases_ms_per_frame` der Trace-Zusammenfassung: ein gemessener Scope oder ein Scope und eine seiner Stufen. Die Zeit eines Scopes umfasst die Scopes und Stufen darin, und gleichzeitig arbeitende Threads zählen jeweils, sodass die Phasen zusammen mehr als `FRAME` ergeben können. Aus den Trace-Summen. |
| `Q0 WAIT MS/FRAME` | Zeit pro Frame, die der Paket-Worker von Queue 0 gewartet hat, in ms: `MEM` in Paketen, die auf Guest-Speicher oder auf das Ende des Renderns warten, `LOCK` auf den GPU-Lock, `GPU` in den Syncs und seriellen Wartevorgängen des Recorders und in den Fence-Waits von Command-Batches. Ein Speicher-Warten, das die GPU synchronisiert, zählt in `MEM` und in `GPU`. Aus den Trace-Summen. |
| `RENDER PASSES/FRAME` | Pro Frame begonnene Render-Passes; ein Draw, der den offenen Pass fortsetzt, beginnt keinen. Aus den Trace-Summen; nicht angezeigt, solange eine `APS5_TRACE_*`-Variable gesetzt ist. |
| `PIPELINES` | `COMPILING`: Shader-Vorbereitungen und Vorab-Builds von Pipeline-Libraries in der Warteschlange oder in Arbeit. `NEW/S`: in der letzten Sekunde erstellte Pipelines, wie `SHADERS` auf Seite 1. `LEFT`: `COMPILING` geteilt durch `NEW/S`, die verbleibenden Sekunden bei der aktuellen Rate; nur angezeigt, solange `NEW/S` über null liegt. Das ist eine Schätzung, und sie steigt, wenn neue Arbeit hinzukommt. |
| `HIT %` | `DRAW CACHE`: der Anteil der Lookups im Draw-Cache in der letzten Sekunde, die einen Eintrag fanden, dessen Stages alle übereinstimmten. `PIPELINES`: der Anteil der Lookups im Pipeline-Cache, die die Pipeline fanden. Jeder Wert wird für eine Sekunde ohne Lookups weggelassen. |
| `FLIPS/S`, `PRESENTS/S` | Die Flips des Spiels pro Sekunde im Vergleich zu den pro Sekunde präsentierten Frames. |
| `DROPPED/S`, `REPEATED/S` | Pro Sekunde: Flips, die nicht präsentiert wurden, weil kein Swapchain-Image frei war (der Zähler `dropped` der Zeile `[present]` von `APS5_PROFILE_DRAW=1`), und Presents desselben Anzeigepuffers wie beim Present davor. |
| `COMMIT` | Die Commit-Belastung des Systems im Vergleich zu seinem Commit-Limit, in GB: `CommitTotal` und `CommitLimit` von `GetPerformanceInfo` unter Windows, `Committed_AS` und `CommitLimit` aus `/proc/meminfo` unter Linux, wo das Limit nur mit `vm.overcommit_memory=2` durchgesetzt wird. Dann `FAULTS/S`, die Page Faults des Prozesses pro Sekunde, einschließlich Soft Faults: `PageFaultCount` von `GetProcessMemoryInfo` unter Windows, Minor- plus Major-Faults von `getrusage` unter Linux und macOS. macOS meldet keine Commit-Belastung, daher zeigt die Zeile dort nur `FAULTS/S`. |
| `HOST IMPORTS` | Verwendete Host-Speicherimporte, dann ihre Größe im Vergleich zum Import-Budget, in GB. Nicht angezeigt auf einem Gerät, das keinen Host-Speicher importieren kann. |
| `CPU COPIES` | Pro Sekunde von adressbasierten Ressourcen-Builds kopierter Guest-Speicher, für die Bereiche, die kein Host-Import und kein Mirror bedient, in MB zu 2^20 Bytes. `CAP`: der größte dieser Builds in der letzten Sekunde im Vergleich zur Kopiergrenze `APS5_ADDRESS_COPY_MAX_MIB` (64 MiB, sofern nicht gesetzt), ab der ein Build das Spiel beendet. |
| `UNIMPLEMENTED` | Aufrufe pro Sekunde an Funktionen, die AnyPS5 nicht implementiert, dann ihre Gesamtzahl seit dem Start. Ein solcher Aufruf wirft einen Fehler, ein Zähler über 0 bedeutet also, dass das Spiel ihn abgefangen hat und weitergelaufen ist. |
| `REFUSED SHADERS` | Shader, deren Draws oder Dispatches übersprungen werden. Nur mit `APS5_SKIP_REFUSED_DRAWS=1` oder `APS5_SKIP_REFUSED_DISPATCHES=1` angezeigt; ohne sie beendet ein abgelehnter Shader das Spiel. |
| `TITLE`, `PRESENT` | Die Titel-ID aus der `param.json` des Spiels und der Present-Modus der Swapchain, so wie `ANYPS5_PRESENT_MODE` ihn nennt. |
| `FPS LIMIT` | Der Wert von `ANYPS5_FPS_LIMIT`, in einer eigenen Zeile nach `TITLE` und `PRESENT` und so formatiert wie `FPS` (`59.94` wird als `59.9` angezeigt), ab 10000 in Tausend. Ohne Limit nicht angezeigt. |

Die Busy-Anteile, die Phasen, die Wartezeiten von Queue 0 und die Render-Passes stammen aus den Summen, die der Trace pro Thread führt. Das Anzeigen von Seite 2 schaltet diese Summen ein, ohne die Event-Buffer eines `APS5_TRACE_FRAMES`-Trace: Solange Seite 2 angezeigt wird, liest jeder gemessene Scope auf den Threads des Treibers zweimal die Uhr und addiert seine Zeit zu den Summen seines Threads, und jeder begonnene Render-Pass wird dort gezählt. Das Verlassen von Seite 2 schaltet die Summen wieder ab. Ein Lauf mit einer `APS5_TRACE_*`-Variable behält seinen Trace wie konfiguriert; Seite 2 zeigt diese Zeilen dann nur, solange dieser Trace aufzeichnet. Solange das Overlay ausgeblendet ist oder Seite 1 zeigt, fügt Seite 2 eine Prüfung pro begonnenem Render-Pass hinzu (dieselbe Prüfung, die jeder gemessene Scope schon macht), einen Zähler pro Thread für die Bytes, die jeder adressbasierte Build kopiert, einen Zähler der Aufrufe nicht implementierter Funktionen und die Adresse des Anzeigepuffers pro Present; ihre Quellen werden nur gelesen, solange sie angezeigt wird.

### Log in eine Datei speichern

Setze `ANYPS5_LOG_FILE=<path>` in der Umgebung einer konvertierten Programmdatei, um ihr Log in einer Datei zu behalten, die du an einen Fehlerbericht anhängen kannst. Die Programmdatei gibt weiterhin alles im Terminal aus; die Datei erhält eine Kopie dessen, was sie auf Standardausgabe und Standardfehler schreibt: die Log-Zeilen der Laufzeitumgebung, die `[tag]`-Meldungen, was das Spiel selbst dort ausgibt, und die Absturz- und Abort-Berichte. `APS5_LOG_TIMESTAMPS` versieht die Zeilen in der Datei so mit Zeitstempeln wie im Terminal. Der Relinker, die Trace-Dateien der `APS5_TRACE_*`-Variablen und die Ausgabe von Programmen, die nicht in diese beiden Streams schreiben, gehören nicht dazu.

```
ANYPS5_LOG_FILE=run.log ./app.elf
```

- Die Datei wird beim Laden der Programmdatei geöffnet und angelegt, wenn sie nicht existiert. Ein relativer Pfad wird relativ zum Arbeitsverzeichnis aufgelöst.
- An die Datei wird angehängt, sie wird nie gekürzt, sodass ein Spiel, das eine andere konvertierte Programmdatei startet, in dieselbe Datei weiterschreibt und ein zweiter Lauf unter dem ersten fortfährt. Jeder Start schreibt nur in die Datei eine Zeile `[log] ANYPS5_LOG_FILE: appending to this file, process <id>`. Lösche die Datei vorher, um ein frisches Log zu beginnen. Im Pfad gibt es keine Ersetzung von `{pid}` oder Datum.
- Ein leerer Wert gilt wie eine nicht gesetzte Variable. Ein Pfad, der sich nicht öffnen lässt (ein fehlendes Verzeichnis, ein Verzeichnis, keine Berechtigung), beendet die Programmdatei, bevor sie läuft, mit Status 1 und einer Meldung, die `ANYPS5_LOG_FILE`, den Pfad und den Fehler des Betriebssystems nennt.
- Eine Zeile, die ein einzelner `write`-Aufruf erzeugt, wird vollständig in die Datei geschrieben: Eine Zeile der Standardausgabe landet nie mitten in einer Zeile des Standardfehlers. Zwei Threads, die Zeilen länger als die atomare Größe der Pipe (512 Bytes unter macOS, 4096 unter Linux) in denselben Stream schreiben, können sich weiterhin vermischen, wie im Terminal. Die beiden Streams werden von zwei Threads kopiert, sodass Zeilen des einen Streams einige Zeilen vor oder nach fast gleichzeitig geschriebenen Zeilen des anderen erscheinen können. Eine letzte Zeile ohne Zeilenumbruch erhält in der Datei einen, wenn der Prozess endet.
- Ein Absturz- oder Abort-Bericht und die Zeilen davor werden in die Datei geschrieben, bevor der Prozess endet. Ein von außen beendeter Prozess, etwa durch `SIGKILL` oder `TerminateProcess`, verliert, was er in den letzten Millisekunden geschrieben hat.
- Die Datei hat keine Größenbeschränkung und wächst, solange das Spiel loggt.
- Wenn die Datei nicht mehr beschreibbar ist, etwa weil der Datenträger voll ist, gibt die Programmdatei eine Zeile dazu aus und macht nur mit der Terminalausgabe weiter.
- Solange die Variable gesetzt ist, führt die Programmdatei zwei zusätzliche Threads aus und hält die Standard-Streams in zwei Pipes. Ein Kindprozess, den das Spiel startet, behält seine Ausgabe in denselben Pipes, sodass Ausgabe, die er nach dem Ende des Spiels schreibt, verloren geht, und unter Linux und macOS beendet ihn das `SIGPIPE` dieses Schreibvorgangs, sofern er das Signal nicht ignoriert. Wenn sich das Spiel über `sceSystemServiceLoadExec` durch eine andere Programmdatei ersetzt, werden zuerst die echten Streams übergeben.
- Ohne Konsole, etwa bei einer Windows-Programmdatei, die ohne Konsole gestartet wurde, erhält nur die Datei das Log.
- Unter Windows wurde die Datei mit einer Konsole, mit in eine Datei umgeleiteter oder über eine Pipe gelesener Ausgabe und ohne Konsole geprüft, und der Pfad des Absturzberichts sowie die Übergabe an eine ersetzende Programmdatei werden von den Tests ausgeführt.

### Fehlerbericht-Paket

Führe `python3 tools/bug_report.py --preview` aus, um die geschwärzten Einträge des Pakets und ihre unkomprimierten Größen aufzulisten.
Die Vorschau erstellt kein ZIP und gibt keine Dateiinhalte aus.

`python3 tools/bug_report.py` sammelt, was ein Fehlerbericht braucht, in einer ZIP-Datei: den Laufbericht `anyps5-report.json`, das Log und die Eingabebelegung, mit geschwärzten persönlichen Daten. Es schreibt außerdem `host.txt` mit Betriebssystem, Rechner und Python-Version sowie eine `README.txt`, die sagt, was geschwärzt wurde.

- Standardmäßig liest es `./anyps5-report.json`, das von `ANYPS5_LOG_FILE` genannte Log und die von `ANYPS5_INPUT_CONFIG` genannte Eingabebelegung (oder `./anyps5-input.ini`) und schreibt `bug-report.zip`. Mit `--report`, `--log`, `--input-config` und `--out` wählst du andere Dateien.
- Das Paket enthält `anyps5-settings.txt` mit geschwärzten Werten der `ANYPS5_*`- und `APS5_*`-Variablen, die in der Umgebung des Paketierbefehls gesetzt sind. Führe es mit denselben Einstellungen aus, mit denen das Spiel gestartet wurde, um diese Einstellungen festzuhalten.
- Wenn der Bericht ein Spiel nennt, sucht das Tool neben der gemeinsamen Belegung nach `anyps5-input.<TITLEID>.ini`, auch wenn die gemeinsame Datei fehlt. Das Paket enthält die Überschreibungen dieses Spiels als `anyps5-input-profile.ini`.
- Das Schwärzen ersetzt dein Home-Verzeichnis durch `~`, deinen Benutzer- und Hostnamen durch `<user>` und `<host>` und IPv4-Adressen durch `<ip>`. Es erkennt nur ganze Wörter, sodass ein Benutzername innerhalb eines anderen Worts nicht geändert wird. Andere persönliche Daten kennt es nicht, etwa einen in ein Spiel eingegebenen Namen, einen Dateinamen, der deinen Namen enthält, oder eine IPv6-Adresse; öffne das Paket und prüfe es, bevor du es anhängst.
- Ein fehlendes Log oder eine fehlende Eingabebelegung wird im Paket weggelassen. Der Laufbericht muss gültiges JSON sein. Ein vorhandenes Paket wird nie überschrieben: Der Befehl endet mit Status 1 und nennt die Datei.

### Statusanzeige des Controllers

Eine kleine Anzeige über dem dargestellten Bild listet die Controller der vier Spielerplätze auf, eine Zeile für jeden Platz, der seit dem Start des Spiels einen Controller hatte (bei nicht gesetztem oder auf 0 gesetztem `ANYPS5_PAD_STATUS` seit dem ersten Druck auf F9, weil Controller vorher nicht verfolgt werden): `P1 DUALSENSE  WIRED`, `P2 XBOX  BATTERY MEDIUM`, `P3 DISCONNECTED`. Sie wird nie dauerhaft angezeigt: Sie verschwindet 4 Sekunden nach dem letzten Ereignis, das sie angezeigt hat. Wurde noch kein Controller gesehen, zeigt sie `NO CONTROLLER`.

Drücke F9 (die Standardbelegung von `ShowPadStatus`), um die Anzeige im Spielfenster einzublenden. Ein erneutes Auslösen der Aktion startet den Vier-Sekunden-Timer neu. Du kannst sie in der [Eingabebelegung](INPUT_MAPPING.de.md) auf eine andere Taste oder eine `PAD:`-Tastenkombination legen. Mit `ANYPS5_PAD_STATUS=1` erscheint sie auch von selbst, wenn ein Controller sich verbindet (auch ein Controller, der beim Start des Spiels schon verbunden ist), sich trennt, auf seinem Platz durch einen anderen ersetzt wird oder sein Akku in eine niedrigere Klasse fällt (voll, mittel, niedrig, leer); jedes neue Ereignis startet die 4 Sekunden neu. Mit `ANYPS5_PAD_STATUS=0` oder nicht gesetzter Variable erscheint die Anzeige nur, wenn du `ShowPadStatus` auslöst. Für diese Variable gilt ein leerer Wert wie ein nicht gesetzter, und jeder andere Wert beendet das Spiel beim Start der Videoausgabe mit einem Fehler, der die Variable nennt.

Eine Zeile besteht aus der Spielernummer (dem Platz des Controllers: Ein Controller, der sich neu verbindet, nimmt den niedrigsten freien Platz, was ein anderer Spieler sein kann), dem Controller-Typ und seiner Stromversorgung. Der Typ ist `DUALSENSE`, `DUALSHOCK 4`, `DUALSHOCK 3`, `XBOX` oder `SWITCH`; bei jedem anderen Controller ist es der Name, den SDL meldet, reduziert auf die Zeichen der Overlay-Schrift und mit einem abschließenden Punkt auf 12 Zeichen gekürzt, oder `PAD`, wenn SDL keinen Namen liefert. Die Stromversorgung ist `WIRED` oder eine der Akkuklassen von SDL, `BATTERY EMPTY` (bis 5%), `BATTERY LOW` (bis 20%), `BATTERY MEDIUM` (bis 70%) und `BATTERY FULL`; ein Controller, dessen Treiber oder Verbindung keinen Stand meldet, zeigt `BATTERY UNKNOWN`. Was gemeldet wird, hängt von SDL, dem Betriebssystem und dem Controller ab.

Die Anzeige verwendet Schrift und Skalierung des [Leistungs-Overlays](#leistungs-overlay) und sitzt in der Ecke, die horizontal gespiegelt zu `ANYPS5_OVERLAY_POS` liegt, standardmäßig oben rechts, sodass sie dieses Overlay nie verdeckt. Solange die Bildschirmtastatur des Systems angezeigt wird, wird die Anzeige nicht gezeichnet, und ihre 4 Sekunden laufen weiter. Screenshots enthalten sie nicht. Solange sie ausgeblendet ist, was immer der Fall ist, sofern F9 oder `ANYPS5_PAD_STATUS=1` sie nicht anzeigt, wird nichts gezeichnet, kein Render-Pass und kein Speicher dafür angelegt, und das dargestellte Bild bleibt unverändert. Weiterhin laufen ein Lock und ein Lesen der Uhr bei jedem Present und in jedem Durchlauf der Video-out-Schleife, um festzustellen, ob die Anzeige gewünscht ist. Mit nicht gesetztem oder auf 0 gesetztem `ANYPS5_PAD_STATUS` und ohne Druck auf F9 wird dafür kein Controller-Name kopiert und kein Akkustand gelesen; mit `ANYPS5_PAD_STATUS=1` und während die Anzeige nach F9 angezeigt wird, werden sie bei jedem Schleifendurchlauf gelesen.

### Dialoge nur für PSN

Manche Systemdialoge brauchen das PlayStation Network: das Profil, die Freundesliste, der Store, der Einladungsdialog, die Facebook-Verknüpfung und der Dialog für autorisierte Apps. AnyPS5 stellt kein PlayStation Network bereit, daher enden diese Dialoge sofort mit derselben Antwort wie bei einem Abbruch, und nichts wird angezeigt. Für den Webbrowser-Dialog ist nicht bekannt, dass er das PlayStation Network braucht, daher sagt sein Hinweis nur, dass die Systemfunktion nicht verfügbar ist.

Setze `ANYPS5_PSN_DIALOGS=show`, um auf dem Bildschirm des Spiels einen Hinweis anzuzeigen, wenn sich einer dieser Dialoge öffnet. Der Hinweis hat eine OK-Schaltfläche und übernimmt den Controller, bis er beantwortet ist. Er wird nicht innerhalb von zwei Sekunden nach dem vorherigen erneut angezeigt und nicht, solange ein anderer Dialog oder Hinweis angezeigt wird. Das Ergebnis für das Spiel ist in beiden Fällen dasselbe. `silent`, eine nicht gesetzte und eine leere Variable zeigen nichts an. Jeder andere Wert beendet das Spiel bei seinem ersten solchen Dialog mit einem Fehler, der `ANYPS5_PSN_DIALOGS` nennt.

### Absturzbericht unter Linux

Unter Linux x86-64 gibt ein `SIGSEGV`, `SIGBUS`, `SIGILL` oder `SIGFPE`, das sonst nichts behandelt, einen Bericht auf stderr aus, und in die Log-Datei, wenn `ANYPS5_LOG_FILE` gesetzt ist, bevor der Prozess mit diesem Signal endet:

- das Signal, sein `si_code`, die Fehleradresse und die Thread-ID;
- `rip`, als Adresse und als gemappte Datei plus Offset in dieser Datei, zum Beispiel `app.elf+0xc77cda` oder `libkernel.prx+0xc7e30`;
- `guest abort()` für `int 0x45` oder `guest assert/break` für `int 0x41`, wenn der General-Protection-Kontext des Kernels und lesbare Instruktionsbytes diesen Interrupt erkennen lassen;
- die sechzehn Allzweckregister;
- die Rücksprungadressen der `rbp`-Frame-Kette, bis zu 32;
- bis zu 24 Stack-Wörter innerhalb von 32 KiB über `rsp`, die auf ausführbare Mappings zeigen.

Mit `--trace-imports` und `APS5_TRACE_CALLS` folgen die letzten verfolgten Aufrufe, wie im Abort-Bericht. Ein Offset in einem Guest-Modul oder der Programmdatei lässt sich an diesem Datei-Offset aus der konvertierten Datei disassemblieren.

### Absturzdiagnose unter Windows

Wenn ein Register auf lesbaren Speicher zeigt, der mit mindestens vier druckbaren ASCII-Zeichen beginnt, entweder als UTF-16LE oder als Einzelbytes, gibt der Absturzbericht diesen Text nach dem Speicherdump der Register aus, als `[r13] u"..."` für UTF-16LE oder `[rdi] "..."` für Einzelbytes. Der Text endet an einem NUL, am ersten Zeichen, das weder druckbares ASCII noch Tab, Zeilenvorschub oder Wagenrücklauf ist, nach 512 Zeichen oder am Ende des lesbaren Speichers. Tabs, Zeilenumbrüche, Anführungszeichen und Backslashes werden maskiert, und `...` nach dem schließenden Anführungszeichen bedeutet, dass kein NUL den Text innerhalb dieser Grenzen beendet hat.

Setze vor dem Start der Programmdatei `APS5_CRASH_INDIRECT_MEMORY=1`, um bis zu 64 Bytes von jedem Zeiger einzubeziehen, der im Speicherdump der Register gefunden wird. Der Bericht beschriftet jeden Block mit seinem Register, dem Zeiger-Offset und der Zieladresse. Nicht zugängliche, unvollständige und ausführbare Ziele werden übersprungen. Dieser zusätzliche Dump ist deaktiviert, wenn die Variable nicht gesetzt oder leer ist.

Setze `APS5_CRASH_STACK_BYTES=<n>`, um nach den Kandidaten für Rücksprungadressen auf dem Stack die Stack-Wörter des abstürzenden Threads ab `rsp` für `n` Bytes auszugeben (höchstens 1048576, dezimal oder hexadezimal mit `0x`), vier Wörter pro Zeile mit ihrem Offset zu `rsp`. Darunter gibt der Bericht für jedes eindeutige Stack-Wort, das auf lesbaren, nicht ausführbaren Speicher außerhalb des Stacks des Threads zeigt, acht Wörter an dieser Adresse aus, beschriftet mit dem Offset des Worts und der Adresse, die es enthält, für bis zu 256 Adressen. Das zeigt die Objekte, die die Aufrufer-Frames gehalten haben, etwa das `this` eines Destruktors oder den Puffer eines Containers. Der Dump stoppt an der ersten nicht lesbaren Stack-Seite, und nichts wird ausgegeben, wenn die Variable nicht gesetzt, leer oder 0 ist.

Der Bericht listet die anderen Threads des Prozesses auf, bis zu 128 davon (eine Zeile sagt, wie viele weggelassen wurden), mit ihrem `rip`, `rsp`, `rbp` und den Rücksprungadressen ihrer `rbp`-Frame-Kette. Ein Thread, dessen Kette weniger als zwei Frames ergibt, etwa einer, der in `ntdll.dll` wartet, oder einer in Code ohne Frame-Pointer, erhält außerdem bis zu 24 Kandidaten für Rücksprungadressen auf dem Stack: die Wörter in den 32 KiB über seinem `rsp`, die auf ausführbaren Speicher zeigen, gelesen, während der Thread angehalten ist. Wie die Kandidaten des abstürzenden Threads sind sie ein Scan, kein Unwind, sodass veraltete Rücksprungadressen beendeter Aufrufe darunter auftauchen können.

### GPU-Submission-Rings und Stall-Dumps

Jede GPU-Queue behält ihre letzten 16 Command-Buffer-Submissions: wann jede übermittelt wurde, ihre Serial, Größe und Guest-Adresse sowie eine Kopie ihrer ersten 512 Dwords nach dem Inlining der Indirect Buffers. `APS5_PM4_RING=<n>` behält stattdessen die letzten `n`, von 0 bis 4096; 0 behält keine, und jeder andere Wert beendet das Spiel bei seiner ersten Submission. Die Rings werden auf stderr ausgegeben, eine Zeile pro Submission und eine pro dekodiertem Paket, wenn der Windows-Absturzbericht läuft: bei einer unbehandelten Exception, `abort()`, einem ungültigen CRT-Parameter oder dem zeitgesteuerten Dump von `APS5_HANG_DUMP_SECS`.

Mit `APS5_STALL_WATCHDOG_SECS=<n>` gibt eine Queue, die `n` Sekunden ohne Fortschritt in einem `WAIT_REG_MEM`, einem `REWIND` oder hinter Queue 0 gewartet hat, die Rings für dieses Warten einmal aus, und das Spiel läuft weiter. Ein `WAIT_REG_MEM` macht Fortschritt, während irgendein anderes Paket läuft oder abgeschlossen wird, wie bei `APS5_GPU_WAIT_TIMEOUT_MS`. `n` liegt zwischen 0 und 86400, und 0, der Standard, deaktiviert den Watchdog; jeder andere Wert beendet das Spiel bei seiner ersten Submission, wie bei `APS5_PM4_RING`. Während die Rings für eine hinter Queue 0 festgehaltene Queue ausgegeben werden, warten neue Submissions und die anderen Queues.

Mit zusätzlich `ANYPS5_STALL_PROMPT=1` zeigt der Host beim ersten Ablaufen eines überwachten Wartens ein Meldungsfenster: `Wait` lässt das Spiel weiterlaufen, und `Quit` beendet es mit Exit-Code 124, nachdem die Rings ausgegeben wurden. Nach `Wait` erscheint das Fenster eine Periode später erneut, höchstens dreimal für dasselbe Warten; die Rings werden in jedem Fall einmal pro Warten ausgegeben. Es ist immer nur ein Fenster offen, und für ein Warten, das es offen vorfindet, wird erst gefragt, nachdem dieses Fenster beantwortet ist. Wenn kein Meldungsfenster angezeigt werden kann, werden die Rings ausgegeben, und das Spiel läuft weiter. Nicht gesetzt oder `0`, der Standard, zeigt kein Fenster; jeder andere Wert als `0` oder `1` beendet das Spiel bei seiner ersten Submission. Ohne die Abfrage beendet der Watchdog nie ein Spiel: Er gibt die Rings einmal pro Warten aus, und das Spiel läuft weiter.

## Exit-Codes

`0`: Konvertierung erfolgreich, oder Hilfe bzw. Version wurde angezeigt. `1`: ungültige Argumente oder ein `--verify-patches`-Eintrag, der nicht übereinstimmt. `2`: Konvertierung gescheitert; der Fehler wird auf stderr ausgegeben. Mit `--autorun` gibt eine erfolgreiche Konvertierung den Exit-Code der gestarteten Anwendung zurück.

## Import-Prüfung

[`tools/import_audit.py`](../../tools/import_audit.py) listet vor dem Start des Spiels die Systemfunktionen auf, die ein konvertiertes Spiel importiert, und ob die gebauten Bibliotheken sie bereitstellen. Es braucht Python 3 und sonst nichts.

```sh
relinker --registry source/input.elf app.elf
python3 tools/import_audit.py app.registry.json app.libc.prx.guest.prx.registry.json --libs build/core/libs/libs --modules source/sce_module
```

`--registry` schreibt `app.registry.json` für die Programmdatei und eine `app.<module>.registry.json` für jedes konvertierte mitgelieferte Modul; übergib alle. Wird die Registry eines Moduls zusammen mit der Registry der Programmdatei übergeben, entnimmt die Prüfung die Importe des Moduls dieser Registry und lässt in diesen Datensätzen den Dateinamen des Moduls weg. Ein ELF-Modul in `--modules` liefert seine Exporte und benötigten Bibliotheken und, wenn seine Registry fehlt, seine Guest-Importe. Ein konvertiertes Windows-PE-`*.guest.prx` liefert nur Exporte. Übergib seine Registry, um seine Guest-Importe einzubeziehen. `--libs` ist das Verzeichnis, das das Target `libs` füllt ([Build-Anleitung](../dev/BUILD.md)); die Exporte werden aus den gebauten `.prx`-Dateien gelesen, sodass das Ergebnis dem entspricht, was der Loader findet. Importe werden einmal pro NID und Bibliothek gezählt, und jede Referenz fällt in genau eine Klasse:

| Klasse | Bedeutung |
|-------|---------|
| `implemented` | Eine gebaute Bibliothek exportiert die NID, und ihre Funktion ist kein werfender Stub. |
| `partial` | Exportiert, aber die Funktion wirft bei Eingaben, deren Verhalten unbekannt ist. |
| `stub` | Exportiert, aber die ganze Funktion ruft `NotImplemented_nid_no_patch` auf: Das Spiel lädt und wirft, wenn es sie aufruft. |
| `absent` | Keine gebaute Bibliothek, die der Loader durchsucht, exportiert die NID: Der Loader scheitert. |
| `module` | Der Import nennt eine Bibliothek des Spiels, die in `--modules` gefunden wurde und das Symbol oder seine NID exportiert. |

Ein Import, der eine Bibliothek nennt, verlangt den wörtlichen Exportnamen. Wird seine Implementierung nur durch Hashen des Importnamens gefunden, zählt er weiterhin als `implemented` oder `stub`, aber der Bericht führt die Namensabweichung auf, setzt `symbol_mismatch` im JSON-Datensatz des Imports, zählt sie in der Zusammenfassung, und die Prüfung endet mit Code 1. Ein starker Import mit Namensabweichung ist ein aktueller Stopp. Importe ohne Bibliothek behalten den Rückgriff der Prüfung vom Namen auf die NID.

Modul-Registries kennzeichnen schwache ELF-Importe mit `"weak": true`, sodass die Auswahl der aktuellen Stopps der Prüfung sie überspringen kann. Starke Importe lassen das Feld weg. Ältere Modul-Registries bewahren schwache Bindungen nicht; erzeuge sie neu, um optionale Importe zu unterscheiden.

ELF-Module in `--modules` werden unabhängig vom Dateinamen geprüft: Ihre undefinierten Symbole werden anhand der gebauten Bibliotheken und der Exporte der anderen Module eingeordnet, wobei der Bericht die Moduldatei nennt, und die Bibliotheken, die sie brauchen, müssen eine Datei in `--libs` oder `--modules` haben. Bei einem Modul, dessen Registry zusammen mit der Registry der Programmdatei übergeben wird, werden die Importe stattdessen aus dieser Registry gezählt und nicht mit seinem Dateinamen gekennzeichnet; die Bibliotheken, die es braucht, werden weiterhin aus seiner Datei gelesen. Gewöhnliche Begleitdateien, die keine Module sind, werden ignoriert; fehlerhafte ELF-Dateien, SELF-Container und Nicht-ELF-Dateien mit Modulendungen bleiben Fehler; die konvertierten `*.guest.prx`-PE-Module eines Windows-Builds werden akzeptiert, jede andere PE-Datei ist ein Fehler. Die Prüfung liest dynamische Tabellen aus den ELF-Section-Headern oder bei Dateien ohne Sections aus `PT_DYNAMIC` und einer SysV- oder GNU-Hash-Tabelle. Sind beide Sichten vorhanden, müssen ihre Bestände an Importen, Exporten und Abhängigkeiten übereinstimmen; widersprüchliche Metadaten sind ein Fehler. Ein PS4-Modul hat kein `DT_STRTAB` oder `DT_SYMTAB`: Die SCE-Tags `0x61000035` und `0x61000039` geben die Offsets seiner String- und Symboltabelle in seinem SCE-Datensegment für dynamische Bibliotheken (`0x61000000`) an, `0x61000037` und `0x6100003b` geben die Größe der String-Tabelle und die Größe eines Symboleintrags an, und `0x6100003f` gibt die Größe der Symboltabelle an. Ein Modul, das sowohl Standard- als auch SCE-Tabellen nennt, ist ein Fehler; das `0x6100003f`, das PS5-Module neben ihren Standardtabellen tragen, ändert nicht, wie sie gelesen werden.

Jede Registry hat eine Begleitdatei mit demselben Präfix und der Endung `.needed.json`, die ihre Host-Bibliotheksabhängigkeiten auflistet, einschließlich deklarierter `DT_NEEDED`-Einträge ohne Importe. Abhängigkeiten, die konvertierte mitgelieferte Module bereitstellen, einschließlich ihrer Aliase, werden weggelassen. Die Begleitdatei der Programmdatei enthält die Host-Abhängigkeiten ihrer mitgelieferten Module. Die Prüfung liest diese Begleitdateien automatisch; lass sie neben den Registries liegen. Ältere Registries ohne Begleitdateien funktionieren weiterhin, können aber keine Abhängigkeiten ohne Importe melden; erzeuge sie mit `--registry` neu, um diese Abhängigkeiten einzubeziehen.

Der Bericht listet auch benötigte Bibliotheken auf, die keine Datei in `--libs` oder `--modules` haben, einschließlich Abhängigkeiten ohne Importe. Diese erhöhen die Zahl der Importe und Referenzen nicht. Windows kann eine fehlende Abhängigkeit überspringen, wenn kein Import sie braucht; Linux braucht die Datei trotzdem. Die Prüfung listet außerdem Importe auf, die nur eine andere Bibliothek als die genannte exportiert; diese werden unter Linux aufgelöst und können unter Windows scheitern.

Bei einem Windows-Build mit Begleitdateien zählt für einen Import ohne Bibliothek nur, was der Entry-Stub durchsucht: die Module des Spiels, die Bibliotheken, die die Begleitdateien auflisten, die Kernel-Familie, wenn eine davon eine Kernel-Bibliothek ist, und `libc.prx`, wenn eine eine Kernel-Bibliothek oder `libSceLibcInternal.prx` ist. Ein Import, den nur andere gebaute Bibliotheken exportieren, ist `absent`; sein JSON-Datensatz führt sie in `unsearched_providers` auf, und Zusammenfassung und Bericht zählen sie pro Bibliothek. Die Begleitdateien aller übergebenen Registries werden zusammengeführt, übergib also die Registries einer Programmdatei und ihrer Module gemeinsam, mit jedem Ordner, der die konvertierten Module des Spiels enthält, in `--modules`: Ein Import, den nur ein Modul des Spiels bereitstellt, zählt als `absent`, wenn der Ordner dieses Moduls fehlt. Wenn keine Begleitdatei eine Bibliothek auflistet, und bei einem Linux-Build, zählt weiterhin jede gebaute Bibliothek.

Die Prüfung durchläuft auch die eigenen Host-Abhängigkeiten der gebauten Bibliotheken: Bei jeder `.prx` unter `--libs` werden die PE-Importe (oder ELF-`DT_NEEDED`-Einträge) auf `.prx`-Namen ohne Datei in `--libs` oder `--modules` geprüft, wobei vorhandenen Bibliotheken transitiv gefolgt wird. Lücken lassen die Prüfung scheitern und werden als `Importer: <root> Chain: <root> -> ... -> <missing>`-Zeilen gemeldet, da der Loader an ihnen scheitert, selbst wenn jeder Import des Spiels aufgelöst wird.

| Option | Wirkung |
|--------|--------|
| `--libs <dir>` | Verzeichnis der gebauten `.prx`. Pflicht; für mehrere wiederholen. |
| `--modules <dir>` | Verzeichnis der eigenen Module des Spiels; für mehrere wiederholen. |
| `--source <dir>` | `core/libs/prx`-Baum, der nach werfenden Stubs durchsucht wird. Standard ist der dieses Repositorys. |
| `--strict-stubs` | Endet zusätzlich mit Code `1`, wenn ein Import die Klasse `stub` hat, schwache Importe und die Importe von `--modules`-Dateien eingeschlossen. `partial`-Importe zählen nicht. |
| `--strict-partial` | Endet zusätzlich mit Code `1`, wenn ein Import die Klasse `partial` hat. Verwende auch `--strict-stubs`, um beide Klassen abzulehnen. |
| `--strict-providers` | Endet zusätzlich mit Code `1`, wenn ein Import, der eine Bibliothek nennt, nur von einer anderen Bibliothek exportiert wird. `module`-Importe und Importe, die `libc.prx` exportiert, zählen nicht. |
| `--names <file>` | NID und Name pro Zeile, wie in `aerolib.csv` aus [`tools/nid_names.py`](../../tools/nid_names.py); benennt die fehlenden Importe. Wird nie heruntergeladen; ein Name, dessen Hash nicht seine NID ergibt, wird markiert. |
| `--json <file>` | Vollständiges Ergebnis, versehen mit dem Commit des Repositorys und der Angabe, ob sein Baum verändert war. |

`nid`-Felder einer Registry können entweder kodierte NIDs oder gewöhnliche ELF-Symbolnamen wie `strcmp` enthalten. Importe werden zuerst mit wörtlichen Exporten abgeglichen, dann mit ihren berechneten NIDs, wenn die wörtliche Suche nichts findet. Berichte behalten den ursprünglichen Importnamen. Dass ein Export vorhanden ist, belegt nicht, dass jeder Plattform-Loader diesen Suchweg unterstützt.

Die Prüfung endet mit `0`, wenn sie bestanden ist. Sie endet mit `1` bei `absent`-Importen, Abweichungen von Importnamen, benötigten Bibliotheken oder Host-Abhängigkeiten von Bibliotheken ohne Datei oder bei Befunden, die `--strict-stubs`, `--strict-partial` oder `--strict-providers` ablehnen. Sie endet mit `2` bei ungültigen Argumenten oder einem Ein- oder Ausgabefehler.

Eine Modul-Registry führt `nid`, `library` und `targetOffset` für jede Relokation gegen eine Funktion auf, die kein anderes mitgeliefertes Modul exportiert, ohne das `#`-Suffix der NIDs der Programmdatei.

## Importe nativer Anwendungen patchen

`nid_patcher --exec <file> [file2 ...]` patcht Importe direkt in der Datei und erhält die Exporte der Anwendung. Es erkennt PE, ELF64 und Mach-O64 am Dateiinhalt. Kein Bibliotheksname ist nötig. Baue für jede Zielplattform eine eigene Programmdatei.

Standardmäßig folgen Importe den Benennungsregeln für Bibliotheksexporte: `_nid_no_patch` und `SDL_*` bleiben unverändert; `_nid_no_patch_cut` wird ohne Hashen entfernt; `_nid_postfix` und numerische `_nid_disambig`-Suffixe werden vor der Berechnung der NID entfernt. Werden sowohl `foo` als auch `foo_nid_postfix` importiert, bleibt `foo` unverändert. Diese Regeln gelten auch für Host-Importe, sofern sie nicht ausdrücklich erhalten werden.

`nid_patcher --exec --only-sce <file>` beschränkt das Umbenennen auf Namen, die mit `sce` beginnen (ohne Beachtung der Groß- und Kleinschreibung) oder auf `_nid_postfix` enden. Beide Modi akzeptieren `--preserve-exports <unpatched-library>` oder `--preserve-export-list <export-list>` für ausdrückliche Ausnahmen. Der vorhandene Modus `nid_patcher <library_name> <file>` bleibt unverändert.

Nicht unterstützte Layouts scheitern, bevor die Eingabedatei geschrieben wird.
