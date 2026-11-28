# Laufbericht

`anyps5-report.json` ist eine strukturierte Zusammenfassung eines Laufs. [`run-report.schema.json`](../run-report.schema.json) ist die einzige maßgebliche Quelle für seine Struktur; die Beispielberichte in [`run-report-fixtures/`](../run-report-fixtures/) sind alle dagegen gültig. Dies ist Version 1. Ein hinzugefügtes Feld behält Version 1 bei; das Umbenennen oder Entfernen eines Felds oder eine Änderung seiner Bedeutung ergibt Version 2.

Gib eine lesbare Zusammenfassung eines Berichts so aus:

```sh
python3 tools/run_report_summary.py path/to/anyps5-report.json
```

Die Zusammenfassung enthält das Ergebnis des Laufs, die am weitesten beobachtete Stufe, Zählwerte zum Rendering, Systemangaben und vorhandene Diagnosen zu fehlenden Funktionen und Shadern. Sie gibt an, wenn ein Abschnitt nicht gesammelt oder eine Liste gekürzt wurde. Das Werkzeug schreibt UTF-8 auf stdout. Es liest den lokalen Bericht und lädt ihn nicht hoch.

Der Befehl endet mit 0, wenn er eine Zusammenfassung ausgibt, auch wenn der im Bericht aufgezeichnete Lauf fehlgeschlagen ist. Er endet mit 2, wenn er den Bericht nicht lesen oder auswerten kann. Prüfe anhand der Zeile `Result` der Zusammenfassung oder des Felds `run.exit` im JSON-Bericht, wie das Spiel geendet hat.

Windows PowerShell 5.1 schreibt standardmäßig UTF-16LE, wenn du einen Befehl mit `>` umleitest. Um die Zusammenfassung als UTF-8 zu speichern, lass `cmd` die Ausgabe umleiten.

```powershell
cmd /d /c 'python tools/run_report_summary.py "path to/anyps5-report.json" > summary.txt'
```

Die Datei bleibt auf deinem Rechner. Es wird nichts hochgeladen, solange du sie nicht teilst.

## Mehrere Berichte auswerten

Übergib die Berichtsdateien an `--triage`, um eine JSON-Übersicht auszugeben, ohne ein Spiel zu starten:

```sh
python3 tools/run_report_summary.py --triage run-a/anyps5-report.json run-b/anyps5-report.json
```

Die Ausgabe `anyps5-run-triage` in Version 1 enthält die Anzahl der Eingabedateien, eindeutige Läufe, doppelte Berichte und Gruppen nach Title-ID, Inhaltsversion des Spiels, Build-Commit, Release und Änderungsstatus. Ein unbekannter Wert bleibt `null`. Diese Gruppen beschreiben die übergebenen Berichte. Sie belegen nicht, dass die Läufe identische Hardware oder Einstellungen verwendet haben.

Jede Gruppe zählt aufgezeichnete Exit-Arten, Stufen und Stoppgründe. Ihre Liste fehlender Funktionen zählt die Läufe, die jede Funktion beobachtet haben, und die Gesamtzahl der aufgezeichneten Aufrufe. Die Abdeckung fehlender Funktionen unterscheidet vollständige Listen, Teillisten und Läufe, die die Daten nicht gesammelt haben. Ein wiederhergestellter oder unvollständiger Lauf oder ein Lauf, der fehlende Funktionen weggelassen hat, hat bei eingeschalteter Sammlung eine teilweise Abdeckung. Nur eine vollständige leere Liste wird in `complete_without_observations` gezählt. Ein fehlender Bericht liefert keine Belege.

Kopien mit derselben Lauf-ID und übereinstimmenden Triage-Daten zählen einmal. Widersprechen sich zwei Berichte bei den Triage-Daten eines Laufs, endet der Befehl mit 2 und nennt die widersprüchliche Eingabedatei anhand ihrer Position. Ein Bericht, den er nicht lesen oder auswerten kann, führt ebenfalls zu Exit 2, ohne teilweise Zusammenfassung. Die Übersicht lässt Lauf-IDs, Dateipfade, Systemangaben und frei formulierte Stoppmeldungen weg.

## Abschalten

Der Bericht wird standardmäßig geschrieben. Schalte ihn mit dem Startparameter `--aps5-report=off` oder der Umgebungsvariable `APS5_REPORT=0` ab. Der Parameter hat Vorrang vor der Umgebungsvariable, und die Umgebungsvariable hat Vorrang vor dem Standard (an). Es gibt keine INI-Einstellung. Der Parameter erfordert ein Spiel, das mit einem ausreichend neuen Relinker konvertiert wurde; die Umgebungsvariable funktioniert auch mit älteren Konvertierungen. Die Laufzeitumgebung liest den Parameter aus der Befehlszeile und entfernt ihn nicht, sodass das Spiel `--aps5-report=...` in seinen Argumenten sehen kann. Startet sich das Spiel selbst neu, wird die Wahl über `APS5_REPORT` weitergegeben.

## Wo er gespeichert wird

`anyps5-report.json` wird neben die Spielprogrammdatei geschrieben. Kann der Bericht dort nicht geschrieben werden, sieh unter `reports/<run id>/` im AnyPS5-Datenordner deines Betriebssystems nach.

- Windows verwendet `%LOCALAPPDATA%\AnyPS5`.
- Linux verwendet `$XDG_DATA_HOME/AnyPS5`, wenn `XDG_DATA_HOME` ein absoluter Pfad ist, sonst `$HOME/.local/share/AnyPS5`.
- macOS verwendet `~/Library/Application Support/AnyPS5`.

Bei einem sauberen Ende zeigt die Erfolgsmeldung nur den Dateinamen des Berichts. Der Name des Ausweichordners ist die `run.id` des Berichts, eine zufällige Zeichenfolge aus 32 Hexadezimalzeichen in Kleinschreibung.

Der Bericht wird einmal geschrieben, wenn der Lauf über `exit` oder `_exit` des Gasts endet, unter Linux und macOS außerdem, wenn der Einstiegspunkt zurückkehrt oder der Host `std::exit` aufruft. Unter Windows schreiben nur `exit` und `_exit` einen Bericht: Eine Rückkehr aus dem Einstiegspunkt schreibt keinen, weil Exit-Handler einer DLL innerhalb des Loader-Locks laufen. Der Code für Windows und macOS und der Junction-Test wurden nicht ausgeführt. Ein Lauf, der an einer Ausnahme stoppt, die nichts abfängt (`std::terminate`), etwa beim Aufruf einer nicht implementierten Funktion, schreibt den Bericht mit Exit `abort`, bevor der Terminate-Handler läuft, oder danach, wenn das Spiel einen eigenen Handler installiert hat. Ein Lauf, der zwangsweise beendet wird oder abstürzt, schreibt keinen Bericht.

## Was `anyps5.modified` bedeutet

`false` bedeutet, dass der Git-Baum des Builds keine Änderung an versionierten Dateien, keine unversionierten Dateien, die Git nicht ignoriert, und keine Änderung innerhalb eines Submoduls hatte. `true` bedeutet eines davon. `null` bedeutet, dass der Build nicht aus einem Git-Checkout stammt. Eine gemischte Sammlung von Bibliotheken aus verschiedenen Builds wird nicht erkannt.

## Unbekannt ist null

Ein Wert, den die Laufzeitumgebung nicht kennen kann, ist `null`, nie eine Schätzung. Das gilt für `anyps5.*`, jedes Feld unter `game.*` und `system.*`, das ganze Objekt `system.gpu`, `run.exitCode`, `stop.source` und Angaben zu Instruktionen. Lies null als „unbekannt“, nicht als false oder 0.

Jeder Textwert im Bericht durchläuft denselben Filter für Benutzernamen, Hostnamen, Adressen und Kontonummern. Eine Version aus vier durch Punkte getrennten Zahlen (`1.2.3.4`) sieht wie eine IPv4-Adresse aus und wird daher weggelassen (null), statt zu riskieren, eine Adresse zu schreiben; Versionen aus drei Zahlen (`01.002.000`) und Release-Tags (`v0.1.3`) bleiben erhalten. Commit und Lauf-ID sind Hexwerte fester Länge und werden nur anhand ihres Musters geprüft, sodass die Build-Identität immer vorhanden ist.

`game.titleId` wird nur übernommen, wenn sie dem Muster von `PPSA` oder `CUSA` entspricht (vier Großbuchstaben und fünf Ziffern); sonst ist sie null.

## Lauf

| Feld | Bedeutung |
|---|---|
| `stage` | Der am weitesten fortgeschrittene Meilenstein, den die Laufzeitumgebung beobachtet hat. `unknown` ist der Startwert. Die Laufzeitumgebung meldet nie `converts`: Die Konvertierung findet im Konverter statt. |
| `loads` | Die Laufzeitumgebung hat den Einstiegspunkt des Spiels erreicht. Das belegt nicht, dass jeder Import aufgelöst wurde, weil Importe später geladen werden können. |
| `starts` | Spielcode wurde in seinem Thread angesprungen. |
| `first-draw` | Ein echter Draw oder Dispatch des Spiels wurde an die GPU übergeben. |
| `presents` | Die GPU hat einen Frame des Spiels zur Anzeige angenommen. Das belegt nicht, dass der Frame sichtbar oder nicht schwarz war. |
| `milestones` | Sekunden seit dem Start. Nicht erreichte Meilensteine werden weggelassen. |
| `exit` | `clean`, `crash`, `abort` oder `killed`. Ein Exit-Code des Spiels ungleich 0 ist nicht automatisch ein Absturz. |
| `exitCode` | Der Exit-Code des Prozesses, wenn bekannt, sonst null. |
| `averageFps`, `fpsWindowSeconds` | Bilder pro Sekunde, gemessen vom ersten bis zum letzten dargestellten Frame, und die Länge dieses Zeitfensters. Beide sind null bei weniger als zwei dargestellten Frames. |
| `frameMs` | Perzentile der Frame-Zeit in Millisekunden: `p50`, `p90` und `p99` der Intervalle zwischen dargestellten Frames, über die letzten 2048 Intervalle. Null bei weniger als zwei dargestellten Frames oder wenn Meilensteine nicht abgedeckt sind. Für `n` sortierte Intervalle ist das Perzentil `p` der Wert an der nullbasierten Position `floor(p * n)` oder der letzte Wert, wenn diese Position hinter dem Ende liegt. Das ist dieselbe Rechnung, die der optionale Leistungs-Trace verwendet. Es ist nicht Nearest-Rank: Bei 10 Intervallen ist `p50` der 6. Wert und `p90` der 10., wo Nearest-Rank den 5. und den 9. ergibt. `tools/presentmon_compare.py` meldet Nearest-Rank, daher kann sein `p99` für dieselben Frames einen Rang niedriger liegen als dieses. |
| `recovered`, `incomplete` | `incomplete` ist true, wenn eine Beobachtung verworfen werden musste. |

## Laufrezept

Neben `anyps5-report.json` schreibt die Laufzeitumgebung `anyps5-recipe.json`, eine kleine JSON-Datei, die festhält, wie der Lauf gestartet wurde, damit sich ein Bericht reproduzieren lässt. Sie enthält `run.id`, `anyps5.commit` (den Build-Commit oder null), `argv` (die Prozessargumente), `environment` (nur die gesetzten `ANYPS5_*`-Variablen) und `configFiles` (die Dateinamen der Konfigurationsdateien, die dem Lauf übergeben wurden, zum Beispiel der Eingabebelegung, ohne ihre Ordner).

Werte durchlaufen denselben Textfilter wie der Bericht, sodass ein Pfad oder ein Benutzername als `[redacted]` erscheint. Eine Variable, deren Name `TOKEN`, `KEY`, `PASSWORD` oder `SECRET` enthält, wird nie geschrieben, unabhängig von ihrem Wert. Das Rezept wird nur geschrieben, wenn der Bericht geschrieben wird; es gelten dieselben Regeln für zwangsweise beendete und abgestürzte Läufe.

## Ende

`stop.kind` ist `none` und `stop.reason` ist `guest-exit` bei einem Lauf, der über den Exit-Pfad geendet hat. Hat der Host das Spiel zuvor zum Beenden aufgefordert, ist `stop.reason` `quit-requested`: Die Meldung lautet `The window was closed.`, wenn das Fenster geschlossen wurde, und besagt, dass der Lauf zum Beenden aufgefordert wurde, ohne dass sein Fenster geschlossen wurde, wenn die Aufforderung auf anderem Weg kam, etwa als SIGTERM von `timeout`, Strg+C oder ein Quit-Befehl. Ein Lauf, der in `std::terminate` geendet hat, hat `stop.reason` `runtime-stop`: `stop.kind` ist `missing-function` mit der Funktion und ihrer `source`, wenn eine nicht implementierte Funktion eine Ausnahme ausgelöst hat, sonst `other` mit dem Text der Ausnahme als Meldung. Das Schema führt die übrigen Arten und Gründe auf; dieser Build schreibt nur diese.

## Was `system` enthält

`system.osVersion` ist Name und Version des Betriebssystems (unter Linux `NAME` und `VERSION_ID` der Distribution aus `os-release` und die ersten drei Zahlen des Kernel-Release, zum Beispiel `Ubuntu 24.04 (kernel 5.15.153)`; was der Hersteller oder der Benutzer an das Kernel-Release angehängt hat, wird weggelassen), `system.cpu` ist der Modellname der CPU (ohne nachgestellte Taktfrequenz wie ` @ 3.50GHz`), `system.ramGiB` ist der installierte Arbeitsspeicher in ganzen GiB, und `system.gpu` ist das Grafikgerät, das die Laufzeitumgebung ausgewählt hat: `vendor` (aus der PCI-Hersteller-ID: `AMD`, `NVIDIA`, `Intel`, `Apple` oder `Other`), `name`, `driver` (der eigene Name und Versionstext des Treibers) und `vulkan` (die API-Version des Geräts). `gpu` wird gesetzt, wenn das Gerät ausgewählt wird, und ist daher im Bericht eines Laufs, der vorher gestoppt hat, null. Dies sollen Modell- und Versionsnamen sein, und sie werden wie jeder andere Text gefiltert: Eine Zeichenfolge, die deinen Anmeldenamen, den Namen deines Home-Ordners, eine Adresse, eine Kontonummer, eine UUID, einen Pfad, eine E-Mail-Adresse oder ein seriennummerähnliches Token enthält, wird aus dem Bericht weggelassen (dieses Feld wird null). Dein Hostname wird ebenfalls verglichen, mit einer Ausnahme: Nur in `system.osVersion` ist ein Hostname erlaubt, wenn er dem Bezeichner einer öffentlichen Distribution aus einer festen Liste im Quellcode entspricht (`ubuntu`, `debian`, `fedora`, `arch`, `cachyos`, `manjaro`, `nixos` usw.) und die `ID` in `os-release` des Rechners derselbe Eintrag ist (ein Rechner namens `cachyos` mit CachyOS behält `CachyOS`). Die `ID` ist Text, den ihr Besitzer bearbeiten kann, daher zählt sie nur, wenn sie sich parsen lässt (passende Anführungszeichen oder keine) und auf der Liste steht; `NAME` und `ID_LIKE` zählen nie. Ein Host, der nach einer Distribution benannt ist, die nicht auf der Liste steht, oder dessen `os-release` etwas anderes angibt, verliert den Betriebssystemnamen in `system.osVersion`. Ein `os-release`-Wert mit einem nicht geschlossenen Anführungszeichen wird nicht verwendet, und die Betriebssystemversion fällt dann auf den Rest zurück (zum Beispiel `Linux (kernel 6.8.0)`). In `cpu`, `gpu.name` und `gpu.driver` wird jeder Name verglichen. Ein Anmeldename erhält diese Ausnahme nie, daher macht ein Anmeldename, der einem Wort des Distributionsnamens entspricht (`arch` unter Arch Linux), `system.osVersion` zu null. Jede Rechner-Zeichenfolge, die deinen Anmeldenamen enthält, wird vollständig geleert, daher leert ein Anmeldename `arc` auch `Intel Arc`. Die Regel für seriennummerähnliche Werte (zwölf oder mehr Buchstaben und Ziffern, mit beidem) verwirft auch eine Treiber-Zeichenfolge mit einem langen Git-Hash, wie sie Entwicklungs-Builds von Mesa enthalten.

## Welche Meilensteine ein System aufzeichnet

| System | `loaded`, `started` | `firstDraw`, `firstPresent` | `stage`, `framesPresented`, Bildrate |
|---|---|---|---|
| Linux | nie aufgezeichnet | aufgezeichnet | aufgezeichnet |
| Windows | nie aufgezeichnet | aufgezeichnet | aufgezeichnet |
| macOS | aufgezeichnet | aufgezeichnet | aufgezeichnet |

`coverage.milestones` ist auf jedem System true; es bedeutet, dass die Stufe, die vom System aufgezeichneten Meilensteine und die Frame-Zähler gesammelt wurden. Lies ein fehlendes `loaded` oder `started` unter Linux oder Windows als „dieses System zeichnet es nicht auf“, nie als „nicht erreicht“: Ein Bericht mit `stage: first-draw` und ohne `loaded` ist ein Lauf, der gezeichnet hat. Der Bericht gibt nur an, was beobachtet wurde: Jeder andere fehlende Meilenstein kann bedeuten, dass der Lauf ihn nicht erreicht hat, oder dass das System ihn nicht aufzeichnet; lies daher die Tabelle, bevor du eine Lücke deutest. Insbesondere `firstDraw` wird nicht durch Compute-Arbeit gesetzt, die einzeln übergeben und abgewartet wird.

## Was die Laufzeitumgebung zählt

- `milestones.firstDraw` wird gesetzt, wenn ein Batch mit einem aufgezeichneten Draw oder Dispatch des Gasts von der GPU-Queue angenommen wurde. Ein Draw des Gasts zählt in beiden Arten von Batches, die die Laufzeitumgebung übergibt; ein Compute-Dispatch des Gasts zählt nur, wenn er im Haupt-Batch der Laufzeitumgebung aufgezeichnet ist (ein Dispatch, der einzeln übergeben und abgewartet wird, setzt den Meilenstein nicht). `firstPresent` und `framesPresented` zählen Anzeigepuffer des Gasts, die die Swapchain-Queue angenommen hat (auch einen suboptimalen); interne Clears, Testpixel und abgelehnte Presents werden nicht gezählt. `loaded` und `started` stammen aus der Einstiegsbrücke der konvertierten Programmdatei; bisher meldet nur die macOS-Brücke sie, daher fehlen diese beiden auf anderen Systemen.
- `missing.functions` zählt Aufrufe einer Funktion, die die Laufzeitumgebung nicht implementiert. Die Bibliothek ist der Ordner, in dem der Stub kompiliert wird; ein Aufruf aus älterem Code, der nicht angibt, wo er erfolgt, hat als Bibliothek null. Der Name ist der exportierte Name ohne das Suffix der Laufzeitumgebung; ein Name, der nicht zum Schema passt, wird nicht aufgeführt und in `truncated.functions` gezählt.
- `shaders.compiled` und `failed` zählen Kompilierungen dort, wo sie stattfinden (einmal pro Variante); die Wiederverwendung einer Variante, im Speicher oder aus dem Disk-Cache, zählt als Cache-Treffer.
- `shaders.compileMs` ist die gesamte in diesen Kompilierungen verbrachte reale Zeit in Millisekunden, fehlgeschlagene eingeschlossen. `shaders.slowCompiles` zählt Kompilierungen, die länger als 100 ms gedauert haben. Beide fehlen, wenn die Shader-Abdeckung aus ist. Sie werden auf dem Rechner gemessen, auf dem das Spiel lief; vergleiche sie daher nur auf demselben Rechner.
- `shaders.unsupported` führt Instruktionen auf, die der Recompiler nicht übersetzen konnte, nach Kodierungsfamilie und numerischem Opcode (der Name ist null). Es führt auch Instruktionen aus Shadern auf, die vorbereitet und nie verwendet wurden, sodass ein Eintrag für sich nicht bedeutet, dass der Lauf daran gestoppt hat; das geben die Stopp-Angaben an.
- `shaders.imageFormats` zählt Formatabfragen des Gasts, interne eingeschlossen, mit der Verwendung `other`, sowie die Formate, die die Laufzeitumgebung abgelehnt hat. Es misst, wie oft ein Format abgefragt wurde, nicht wie viele Bilder es verwendet haben. `shaders.descriptors` zählt die Bindings von Shadern, wie sie für einen Draw vorbereitet wurden; die Zahl misst Nutzung, nicht verschiedene Shader.
- `truncated` zählt verworfene Einträge (eine Funktion, eine Instruktion, ein Format, eine Binding-Art), jeden einmal, gleich wie viele Aufrufe er hatte. Musste ein Hook eine Beobachtung aufgeben, weil der Collector belegt war, ist der zugehörige Zähler mindestens 1 und der Bericht ist als `incomplete` markiert.
- Nichts in dieser Liste wird gesammelt, gezählt oder durchsucht, wenn der Bericht aus ist.

## Listen, Abdeckung und Kürzung

- `missing.functions[]`: `library` kann null sein. `calls` zählt Aufrufe, die die fehlende Funktion erreicht haben. Der erste beendet meist den Lauf, daher ist 1 häufig; dies ist keine Liste von allem, was nicht implementiert ist.
- `shaders.compiled` und `failed` zählen echte Kompilierungen; Cache-Treffer sind keine Kompilierungen (`cacheHits` ist optional). `unsupported[]` hat `encoding`, `opcode` (Name) und `modifiers`, jeweils null, wenn sich der Fehler nicht einer einzelnen Instruktion des Gasts zuordnen lässt. Ein unbekannter numerischer Opcode behält seine Zahl in `opcodeValue`, mit `opcode` null.
- `shaders.imageFormats[]` und `descriptors[]` haben `status` (`accepted`, `rejected` oder `unknown`), `usage` und `count`. `count` zählt Prüfungen, nicht Pixel. „accepted“ belegt nicht, dass jede Verwendung funktioniert.
  - `usage` eines Bildformats: `sampled`, `storage`, `color-target`, `depth-target`, `transfer`, `other`.
  - `usage` eines Deskriptors ist der Zugriff: `read`, `write`, `atomic`, `other`.
- `coverage` gibt an, welche Abschnitte die Laufzeitumgebung für diesen Lauf gesammelt hat. Eine Liste kann leer sein, weil nichts passiert ist oder weil dieser Build sie nicht sammelt. Ist die Abdeckung eines Abschnitts aus, lies seine leere Liste nicht als „nichts fehlt“.
- `truncated` zählt die aus jeder begrenzten Liste verworfenen Einträge, sodass eine begrenzte Liste nie vollständig wirkt.

Listen sind nach Anzahl sortiert, höchste zuerst. Sie enthalten höchstens 200 Einträge (`missing.functions`, `shaders.unsupported`) oder 100 (`imageFormats`, `descriptors`); Zeichenfolgen sind höchstens 300 Zeichen lang. Die ganze Datei umfasst höchstens 65536 Byte UTF-8. Wäre sie größer, verwirft die Laufzeitumgebung zuerst die am niedrigsten eingestuften Listeneinträge, dann optionale Beschreibungen, und nie die Angaben zu Build, Spiel oder Stopp.

## Datenschutz

Nur bekannte, strukturierte Werte werden übernommen. Frei formulierter Text, für den die Laufzeitumgebung nicht bürgen kann, wird weggelassen (null). Die Datei enthält keine Benutzernamen, Home-Pfade, Hostnamen, IP- oder MAC-Adressen, Seriennummern, Konto-IDs, Spielstände oder Screenshots. Pfade sind relativ zum AnyPS5-Quellbaum.
