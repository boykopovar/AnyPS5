# Eingaben über Controller, Tastatur und Maus

AnyPS5 akzeptiert per SDL zugeordnete Gamecontroller sowie seine Standardeingaben für Tastatur und Maus. Bis zu 4 Controller werden gleichzeitig unterstützt. Standardmäßig belegen Controller die Spieler 1 bis 4 in der Reihenfolge ihrer Erkennung (entsprechend den Benutzern von `libSceUserService`). Nur der Controller auf dem Platz von Spieler 1 wird mit Tastatur- und Mauseingaben kombiniert. Siehe [Feste Spielerzuordnung](#feste-spielerzuordnung), um Plätze per Seriennummer zu reservieren. Controller im Xbox-Stil und von SDL erkannte PlayStation-Controller verwenden das Standard-Tastenlayout der PS; Sticks und analoge Trigger werden durchgereicht, und Controller lassen sich verbinden und trennen, während das Spiel läuft. Ohne Konfigurationsdatei verwendet AnyPS5 seine Standardeingaben. Um Eingaben zu ändern, lege `anyps5-input.ini` neben der erzeugten Spielprogrammdatei an. Setze `ANYPS5_INPUT_CONFIG`, um eine Datei unter einem anderen Pfad zu verwenden. Sowohl der Spielordner als auch dieser Pfad dürfen beliebige Zeichen enthalten, etwa `ü` oder `日本`. Mit `ANYPS5_PAD_PORT0_STATE=controller` erreichen Tastatur- und Mauseingaben das Spiel nur, solange Spieler 1 ein Controller zugeordnet ist; siehe [Controller-Verbindung und Benutzer 0](USAGE.de.md#controller-verbindung-und-benutzer-0).

Bei einem Controller mit Xbox-Layout werden A/B/X/Y auf Cross/Circle/Square/Triangle, LB/RB auf L1/R1 und LT/RT auf analoges L2/R2 abgebildet. Start wird auf Options abgebildet. Steuerkreuz und Stick-Klicks werden unterstützt. Bei einem PlayStation-Controller werden Klicks auf das Touchpad und die Positionen von bis zu zwei Fingern auf dem Touchpad weitergegeben. Bei anderen Controllern drückt View (Back) das Touchpad in seiner Mitte.

Eingaben für Tastatur, Maus, Mausrad und Hotkeys haben die Form `Action = Type:Value`. Bei Aktionsnamen wird Groß- und Kleinschreibung nicht unterschieden. Ein `#` oder `;` leitet einen Kommentar ein. Die erste Zeile für eine Aktion ersetzt deren Standardeingaben; weitere Zeilen für dieselbe Aktion fügen alternative Eingaben hinzu. Doppelte Einträge für dieselbe Aktion werden ignoriert. Aktionen, die in der Datei fehlen, behalten ihre Standardeingaben. UTF-8-Dateien mit und ohne Byte-Order-Mark werden unterstützt.

Unterstützte Eingabequellen:

- `KEY:Return`, `KEY:Space` oder ein anderer von SDL akzeptierter Tastenname.
- `MOUSE:Left`, `MOUSE:Middle`, `MOUSE:Right`, `MOUSE:X1` oder `MOUSE:X2`.
- `WHEEL:Up`, `WHEEL:Down`, `WHEEL:Left` oder `WHEEL:Right` für Pad-Tasten. `Left` und `Right` sind horizontale Scrollschritte, etwa die seitlichen Klicks eines kippbaren Mausrads; standardmäßig verwendet keine Aktion sie.
- `PAD:Button` oder `PAD:Button+Button+...` nur für Emulatoraktionen: eine Kombination von Controller-Tasten, die alle gleichzeitig gehalten werden. Die Tastennamen sind `Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left` und `TouchPad`. Die Kombination löst einmal aus, wenn ihre letzte Taste gedrückt wird, und die Tasten erreichen weiterhin den Titel. `PAD:` wird für `ToggleFullscreen`, `TogglePerformanceOverlay`, `Screenshot`, `ShowPadStatus`, `VolumeUp`, `VolumeDown`, `ToggleMute` und `ReloadInputMapping` akzeptiert; andere Aktionen lehnen es ab. Beispiel: `ToggleFullscreen = PAD:Options+R1`.

Solange ein Titel eine Maus geöffnet hat (`sceMouseOpen`) und das Fenster den Fokus hat, blendet AnyPS5 den Host-Cursor aus und erfasst ihn, sodass nur der eigene Cursor des Titels zu sehen ist; die Erfassung endet, wenn das Fenster den Fokus verliert oder wenn der Maus-zu-Stick-Schalter (`ToggleMouse`) an ist. Solange der Titel die Maus hat, gehen Mausklicks und das Mausrad an den Titel statt an ihre Pad-Eingaben. Wird `ToggleMouse` mit seiner Taste (standardmäßig F7) eingeschaltet, geht die ganze Maus an das Pad, bis es wieder ausgeschaltet wird: Bewegung bewegt den Stick, Klicks und das Mausrad drücken ihre Pad-Eingaben, und die Maus des Titels meldet keine Bewegung, Klicks oder Raddrehungen. Ein Titel, der die Maus öffnet, sie im Spiel aber ignoriert, lässt sich so spielen. Mausbewegungen erreichen den Titel vom dargestellten Bild des Fensters auf die Anzeigeauflösung des Titels skaliert, sodass sich der Cursor des Titels so weit bewegt, wie es der Host-Cursor täte.

Die Fenster-Hotkeys (`ToggleFullscreen`, `TogglePerformanceOverlay`, `Screenshot`, `ShowPadStatus`, `VolumeDown`, `VolumeUp` und `ToggleMute`) akzeptieren Tastaturtasten und `PAD:`-Kombinationen. Die Tasten einer Kombination erreichen weiterhin den Titel, wenn der Hotkey auslöst. Eine Taste, die einem Hotkey und einer anderen Aktion zugeordnet ist, löst wie bei jeder anderen Aktion beide aus; die Standardtasten der obigen Aktionen überschneiden sich nicht.

Eingaben für Gamecontroller sind automatisch aktiv. Überschreibungen für Tastatur und Maus gelten unabhängig von der [Neubelegung der Controller-Tasten](#controller-tasten-neu-belegen).

Unterstützte Aktionen sind `Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left`, `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown`, `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown`, `TouchLeft`, `TouchRight`, `StickModifier`, `Shake`, `ToggleMouse`, `ToggleFullscreen` (F11), `ToggleBorderless` (F2, schaltet das Anzeigefenster zwischen normalem und randlosem Fenstermodus um, wie in [USAGE](USAGE.de.md) beschrieben), `TogglePerformanceOverlay` (F10, das [Leistungs-Overlay](USAGE.de.md#leistungs-overlay)), `Screenshot` (F12, speichert das nächste angezeigte Bild wie unter [Screenshots](USAGE.de.md#screenshots) beschrieben) und `ShowPadStatus` (F9, zeigt die [Statusanzeige des Controllers](USAGE.de.md#statusanzeige-des-controllers) für einige Sekunden). `ToggleFullscreen`, `TogglePerformanceOverlay`, `Screenshot` und `ShowPadStatus` akzeptieren auch `PAD:`-Kombinationen. `ToggleBorderless` akzeptiert nur Tastaturtasten.

### Erkennung und Layout von SDL-Controllern

Wenn SDL einen Joystick erkennt, das Spiel aber `NO CONTROLLER` anzeigt, oder Steuerkreuz oder Tasten falsche Eingaben melden, gib ein SDL2-Mapping für die GUID und das Betriebssystem des Controllers an. USB und Bluetooth können unterschiedliche Mappings verwenden. Lade `gamecontrollerdb.txt` von [SDL GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB) herunter oder speichere die Mapping-Zeile aus dem SDL2-Werkzeug `controllermap` in einer reinen Textdatei, einschließlich ihres Felds `platform:`.

Liegt `gamecontrollerdb.txt` neben dem konvertierten Spiel, starte das Spiel aus diesem Ordner. Unter Windows gilt die Einstellung für Programme, die aus derselben PowerShell-Sitzung gestartet werden, bis diese geschlossen wird.

```powershell
$env:SDL_GAMECONTROLLERCONFIG_FILE = (Resolve-Path -LiteralPath '.\gamecontrollerdb.txt').Path
.\app.exe
```

Unter Linux setze die Variable für den Spielprozess.

```sh
SDL_GAMECONTROLLERCONFIG_FILE="$PWD/gamecontrollerdb.txt" ./app.elf
```

Drücke F9, um die [Statusanzeige des Controllers](USAGE.de.md#statusanzeige-des-controllers) zu prüfen. SDL lädt die Datei beim Start. Ein passender Eintrag in der Datei überschreibt das Standard-Mapping von SDL, und ein direkt in `SDL_GAMECONTROLLERCONFIG` gesetztes Mapping hat Vorrang vor der Datei. Die Einträge `SDLButton:` in `anyps5-input.ini` ordnen die resultierenden SDL-Tasten dann PlayStation-Tasten zu.

### Controller-Tasten neu belegen

Verwende `SDLButton:button = Button`, um eine SDL-Controller-Taste einer PlayStation-Taste zuzuordnen. Zum Beispiel tauscht Folgendes Cross und Circle und deaktiviert dann die Back-Taste.

```ini
SDLButton:a = Circle
SDLButton:b = Cross
SDLButton:back = None
```

Jede SDL-Taste darf einmal pro Datei vorkommen. Eine weggelassene Taste behält ihre Standardzuordnung oder ihre Zuordnung aus der gemeinsamen Datei. Diese Überschreibungen gelten für alle Controller-Plätze. Bei SDL-Tastennamen wird Groß- und Kleinschreibung nicht unterschieden; sie umfassen je nach Gerät `a`, `b`, `x`, `y`, `back`, `start`, `leftshoulder`, `rightshoulder`, `leftstick`, `rightstick`, `dpup`, `dpdown`, `dpleft`, `dpright`, `guide`, `misc1`, `paddle1` bis `paddle4` und `touchpad`. Das Ziel akzeptiert dieselben Tastennamen wie `PAD:`-Kombinationen oder `None`. Eine Zuordnung auf `L2` oder `R2` erzeugt einen vollen Triggerdruck. Sticks und physische analoge Trigger verhalten sich wie gewohnt. `PAD:`-Hotkeys folgen den resultierenden Zuordnungen der PlayStation-Tasten.

### Belegung neu laden

`ReloadInputMapping` (standardmäßig F1) liest `anyps5-input.ini` und das Profil des Titels erneut ein, während das Spiel weiterläuft, sodass du eine Änderung ausprobieren kannst, ohne den Titel neu zu starten. Die neue Datei wird vollständig gelesen, bevor sich etwas ändert: Enthält sie einen Fehler, bleibt die bisherige Belegung aktiv, und das Log nennt Datei, Zeile und Grund. Ein erfolgreiches Neuladen ersetzt die gesamte Belegung auf einmal.

Setze `ANYPS5_INPUT_WATCH=1`, um automatisch neu zu laden, wenn sich die Datei oder das Profil des Titels ändert. AnyPS5 prüft Änderungszeit und Größe etwa einmal pro Sekunde in einem Hintergrund-Thread und lädt erst neu, wenn eine Änderung eine Prüfung lang unverändert geblieben ist, damit eine Datei, die noch gespeichert wird, nicht halb geschrieben gelesen wird. Die Option ist standardmäßig aus.

Eine Taste, die `ShowPadStatus` und einer anderen Aktion zugeordnet ist, löst wie bei jeder anderen Aktion beide aus. F9 ist die Standardtaste von `ShowPadStatus`: Wenn du `Screenshot`, `TogglePerformanceOverlay` oder eine andere Aktion auf F9 gelegt hast, belege auch `ShowPadStatus` mit einer Taste, die du nicht verwendest, sonst zeigt jeder Druck auch die Statusanzeige des Controllers. Der [Web-Konfigurator](#web-konfigurator) warnt vor einer Taste, die sich zwei Aktionen teilen.

Drei weitere Aktionen steuern die Ausgabelautstärke des Hosts: `VolumeDown` (F5), `VolumeUp` (F6) und `ToggleMute` (F8), beschrieben unter [Lautstärke und Stummschaltung des Hosts](USAGE.de.md#lautstärke-und-stummschaltung-des-hosts). Sie akzeptieren auch `PAD:`-Kombinationen.

### Standardbelegung

Jede Aktion behält ihre Standardeingaben, sofern die gemeinsame Datei oder das [Profil pro Spiel](#profile-pro-spiel) des aktuellen Titels sie nicht überschreibt.

| Aktion | Tastatur und Maus |
|--------|--------------------|
| `Cross` | Return, Space |
| `Circle` | C |
| `Triangle` | I |
| `Square` | Linke Maustaste |
| `L1` | Q |
| `R1` | E, Left Alt, Right Alt |
| `L2` | R, Maustaste 4 (X1) |
| `R2` | Rechte Maustaste |
| `L3` | Left Shift, Right Shift |
| `R3` | Left Ctrl, Right Ctrl |
| `Options` | Escape |
| `Up`, `Right`, `Down`, `Left` | Pfeiltasten; das Mausrad drückt außerdem `Up` (Rad nach oben) und `Down` (Rad nach unten) |
| `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown` | A, D, W, S |
| `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown` | F, H, T, G |
| `TouchLeft`, `TouchRight` | Backspace, Tab |
| `Shake` | X (meldet, solange gehalten, eine schwingende Beschleunigung und Winkelgeschwindigkeit) |
| `ToggleMouse` | Mittlere Maustaste, F7 |
| `ToggleFullscreen` | F11 |
| `ToggleBorderless` | F2 |
| `TogglePerformanceOverlay` | F10 |
| `Screenshot` | F12 |
| `ShowPadStatus` | F9 |
| `VolumeDown` | F5 |
| `VolumeUp` | F6 |
| `ToggleMute` | F8 |
| `ReloadInputMapping` | F1 |

Zum Beispiel legt Folgendes Cross auf F oder Space, verlegt den linken Stick auf IJKL, verwendet die Maustasten für Square und R2, macht die mittlere Maustaste zur einzigen Eingabe für `ToggleMouse` (die Zeile ersetzt F7) und behält alle anderen Standardeingaben:

```ini
Cross = KEY:F
Cross = KEY:Space
LeftStickLeft = KEY:J
LeftStickRight = KEY:L
LeftStickUp = KEY:I
LeftStickDown = KEY:K
Square = MOUSE:Left
R2 = MOUSE:Right
ToggleMouse = MOUSE:Middle
```

Dieselbe Datei akzeptiert außerdem diese Einstellungen, jeweils höchstens einmal, in der Form `Setting = Value`:

| Einstellung | Wert | Standard |
|---------|-------|---------|
| `LeftStickDeadzone`, `RightStickDeadzone` | Ganzzahliger Prozentsatz von 0 bis 90 des Stickwegs am Controller, der als Mitte gilt. Außerhalb davon wird der Weg neu skaliert, sodass der Stick weiterhin seinen vollen Bereich erreicht. Stick-Eingaben über Tastatur und Maus sind nicht betroffen. | `0` |
| `MouseSensitivity` | Zahl größer als 0 und höchstens 10. Bei 1 neigt eine Mausbewegung von 3000 Counts pro Sekunde den Stick voll, etwa 5 cm pro Sekunde bei 1600 DPI; eine größere Zahl erfordert weniger Bewegung. | `1` |
| `MouseStickTarget` | `Left` oder `Right`. Wählt, welcher Analogstick durch Mausbewegung gesteuert wird, wenn `ToggleMouse` aktiviert ist. | `Right` |
| `MouseVerticalScale` | Zahl von 0.1 bis 10, die die vertikale Mausgeschwindigkeit multipliziert, für Spiele, die die Kamera langsamer oder schneller neigen als drehen. | `1` |
| `MouseCurve` | Zahl von 0.2 bis 5, der Exponent der Kennlinie. Über 1 ergibt feinere Kontrolle bei niedriger Geschwindigkeit; unter 1 erreicht große Neigungen früher. | `1` |
| `MouseDeadzoneOffset` | Zahl von 0 bis 0.9, die kleinste Neigung, die eine Bewegung erzeugt, als Anteil der vollen Neigung. Setze sie knapp über die Deadzone des Spiels: Erhöhe sie, wenn langsame Bewegung die Kamera nicht bewegt, senke sie, wenn kleine Bewegungen springen. | `0.2` |
| `MouseMaxSpeed` | Zahl von 0.1 bis 10, die höchste berücksichtigte Geschwindigkeit als Vielfaches der Geschwindigkeit für volle Neigung. Über 1 hält ein schneller Ruck den Stick etwas länger auf voller Neigung, nachdem die Maus anhält. | `1` |
| `MouseSmoothingMs` | Zahl von 0 bis 500, die Zeitkonstante der Glättung in Millisekunden. Eine Maus, die weniger als 300-mal pro Sekunde meldet, etwa eine Maus mit 125 oder 250 Hz, wird über mindestens drei ihrer Meldeintervalle geglättet, bis zu 30 ms; die Melderate wird aus der Bewegung selbst gemessen. 0 folgt jeder Mausmeldung direkt und schaltet auch dies ab. | `10` |
| `MouseDecayMs` | Zahl von 1 bis 1000, die Millisekunden ohne Bewegung, nach denen der Stick zentriert wird. | `30` |
| `StickModifierScale` | Zahl von 0.05 bis 1, die Neigung von Stick-Eingaben über die Tastatur, solange eine `StickModifier`-Eingabe gehalten wird. | `0.5` |
| `KeyboardRampMs` | Zahl von 0 bis 2000, die Millisekunden, die eine Stick-Eingabe über die Tastatur braucht, um den Stick aus der Mitte auf volle Neigung zu bewegen. 0 neigt sofort. | `0` |
| `TriggerRampMs` | Zahl von 0 bis 2000, die Millisekunden, die eine `L2` oder `R2` zugeordnete Taste oder Maustaste braucht, um den Trigger von 0 auf voll zu drücken. 0 drückt ihn sofort voll. | `0` |
| `ToggleActions` | Kommagetrennte Pad-Tasten (`Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left`), die jeweils von den ihnen zugeordneten Tasten und Maustasten umgeschaltet werden. Stick-Richtungen und Emulatoraktionen werden abgelehnt. Siehe [Umschaltbare Tasten](#umschaltbare-tasten). | keine |

```ini
LeftStickDeadzone = 12
RightStickDeadzone = 8
MouseSensitivity = 2.5
```

## Controller-Zeitleisten

Setze `APS5_PAD_INPUT_FILE` wie unter [Controller-Eingaben für Testläufe](USAGE.de.md#controller-eingaben-für-testläufe) beschrieben und reihe dann eine ganze Tastenfolge ein, nachdem die Anwendung begonnen hat, ihren Controller zu lesen. Speichere eine JSON-Datei wie `menu.json`:

```json
{"version":1,"events":[{"at_ms":200,"hold_ms":300,"buttons":["cross","r1"]},{"at_ms":1000,"hold_ms":300,"buttons":["down"]}]}
```

```sh
python3 tools/pad_timeline.py menu.json /tmp/anyps5-pad --repeat 3 --gap-ms 500
```

Ereignisse beginnen relativ zu dem Zeitpunkt, an dem die Anwendung den neuen Befehl zum ersten Mal sieht. Tastenlisten unterstützen jeden Namen aus `pad_input.py`; gleichzeitige Namen werden kombiniert. Eine leere Liste oder `["release"]` injiziert keine Tasten. Ein Ereignis kann außerdem Positionen für `left_stick` und `right_stick` als `[x, y]` von 0 bis 255 enthalten (128 ist die Mitte); nennt ein Ereignis nur einen der beiden Sticks, bleibt der andere für dieses Ereignis zentriert, und ein Ereignis ohne beide behält die physischen Sticks bei, zum Beispiel `{"at_ms":0,"hold_ms":400,"buttons":[],"left_stick":[255,128]}`, um den linken Stick nach rechts zu drücken. Ereignisse müssen geordnet sein, dürfen sich nicht überlappen, müssen 1–5000 ms halten und innerhalb einer Stunde enden. Wiederholung fügt nach dem letzten Ereignis den angegebenen Abstand ein; nach der Wiederholung werden höchstens 4096 Ereignisse akzeptiert, und die daraus erzeugte Befehlsdatei darf die 64 KiB nicht überschreiten, die die Laufzeitumgebung liest. Das Hilfsprogramm prüft die gesamte Folge, bevor es die Befehlsdatei atomar ersetzt.

Die Laufzeitumgebung wertet geplante Ereignisse bei jedem Lesen des Controllers aus, einschließlich ihres automatischen Loslassens. Ein neuer Impuls oder eine neue Zeitleiste ersetzt die verbleibende Folge. Physische Controller-Tasten kommen weiterhin zu den injizierten Tasten hinzu. Befehle, die beim ersten Lesen des Controllers durch die Anwendung bereits vorliegen, werden ignoriert. Dies plant Eingaben nach realer Zeit: Ereignisse können verpasst werden, solange die Anwendung ihren Controller nicht liest, und es reproduziert weder Frame-Timing noch Spielzustand. Wähle Abstände und Haltezeiten lang genug für die Eingabeschleife des Titels.

### Automatische Spieldurchläufe

`tools/title_run.py` reiht eine Zeitleiste für einen Titel erst ein, wenn sowohl die Mindestverzögerung `after_s` verstrichen ist als auch ein frischer Eintrag der Aufzeichnung von Controller-Port 0 vorliegt. So wird eine während des Starts eingereihte Zeitleiste beim ersten Lesen des Controllers nicht als alter Befehl verworfen. Bei skriptgesteuerten Eingaben ist ein fehlendes oder leeres `APS5_PAD_RECORD` standardmäßig `pad-record.txt` im Trace-Ordner des Laufs. Nicht leere Werte bleiben erhalten; relative Pfade werden im Arbeitsordner des Titels aufgelöst. Aufwärmphase und Erfassung warten jeweils auf einen neuen Eintrag, und `pad_queued_s` bleibt null, wenn der Prozess vor der Bereitschaft endet.

## Controller-Eingaben aufnehmen und abspielen

Setze `APS5_PAD_RECORD` auf einen Dateipfad, um aufzuzeichnen, was ein Titel von Controller-Port 0 liest, während du spielst: jede Änderung seiner Tasten, Sticks und analogen Trigger, unabhängig vom erzeugenden Gerät, in Millisekunden ab dem ersten Lesen des Controllers durch den Titel. Setze bei einem späteren Lauf `APS5_PAD_REPLAY` auf diese Datei, um dieselben Zustände zu denselben Zeiten ab dem ersten Lesen dieses Laufs wieder einzuspeisen, ohne Gerät. Eine Wiedergabe ersetzt bei jedem Lesen Tasten, Sticks und analoge Trigger von Port 0; nach der letzten aufgezeichneten Änderung behält sie den letzten Zustand. Ein Aufzeichnungspfad, der nicht geschrieben werden kann, oder eine Wiedergabedatei, die nicht gelesen werden kann oder nicht im unten beschriebenen Format vorliegt, beendet den Titel beim ersten Lesen des Controllers mit einem Fehler, der die Variable nennt.

```sh
APS5_PAD_RECORD=/tmp/fight.txt ./app.elf
APS5_PAD_REPLAY=/tmp/fight.txt APS5_NO_PAD_INPUT=1 ./app.elf
```

Die Datei beginnt mit der Zeile `APS5_PAD_RECORDING 1`, gefolgt von einer Zeile pro Änderung: die Zeit in Millisekunden, die Tasten hexadezimal (die Werte aus `pad_input.py`), x und y des linken und rechten Sticks sowie L2 und R2, jeweils 0 bis 255. Eine Wiedergabe folgt den aufgezeichneten Zeiten, nicht den Frames des Titels: Ein Lauf, der langsamer lädt oder mit einer anderen Bildrate läuft, oder ein Titel, der andere Zufallszahlen zieht, weicht von der aufgezeichneten Sitzung ab; vergleiche Läufe daher nach Szene statt nach Sekunde.

## Vibrationsstärke des Controllers

Vibration ist nicht Teil der Belegungsdatei. Die hostseitige Stärke der Vibration eines Controllers wird mit `ANYPS5_RUMBLE` eingestellt, beschrieben unter [Vibrationsstärke des Controllers](USAGE.de.md#vibrationsstärke-des-controllers); `0` schaltet sie für jeden verbundenen Controller ab. Die Vibration pausiert außerdem, solange das Fenster keinen Fokus hat, sofern nicht `ANYPS5_RUMBLE_ON_FOCUS_LOSS=0` gesetzt ist, siehe [Vibration ohne Fensterfokus](USAGE.de.md#vibration-ohne-fensterfokus).

Der Erkennungsimpuls, an dem sich die Controller der Spieler unterscheiden lassen, wird mit `ANYPS5_PAD_IDENTIFY` eingestellt, beschrieben unter [Erkennungsimpuls des Controllers](USAGE.de.md#erkennungsimpuls-des-controllers).

## Feste Spielerzuordnung

Standardmäßig belegen verbundene Controller den ersten freien Spielerplatz in der Erkennungsreihenfolge von SDL. Damit ein Controller nach erneutem Verbinden oder einem Neustart beim selben Spieler bleibt, setze `ANYPS5_PAD_PLAYER_1_SERIAL` bis `ANYPS5_PAD_PLAYER_4_SERIAL` auf die exakte SDL-Seriennummer für jeden Spieler. Setze `ANYPS5_PAD_LOG_CONTROLLER_SERIAL=1`, um während der Einrichtung Seriennummern ins Verbindungslog aufzunehmen; das Protokollieren von Seriennummern ist standardmäßig aus. SDL stellt möglicherweise nicht für jeden Controller oder jedes Backend eine Seriennummer bereit. Solche Controller und Controller, deren Seriennummer nicht konfiguriert ist, verwenden den ersten freien, nicht reservierten Spielerplatz. Ein konfigurierter Platz bleibt reserviert, solange sein Controller getrennt ist, damit andere Controller nicht den Platz dieses Spielers einnehmen. Wird dieselbe Seriennummer mehreren Spielern zugewiesen, bricht der Start mit einem Fehler ab.

Zum Beispiel, um zwei Controller zuzuordnen und die übrigen Spielerplätze automatisch zu lassen:

```sh
ANYPS5_PAD_PLAYER_1_SERIAL=ABC123 ANYPS5_PAD_PLAYER_2_SERIAL=XYZ789 ./app.elf
```

Seriennummern können sich unterscheiden, wenn ein Controller über eine andere Verbindungsart angeschlossen oder von einem anderen SDL-Backend bereitgestellt wird. Prüfe im Verbindungslog den exakten Wert, den der aktuelle Host meldet.

## Analoge Tastatureingabe

`StickModifier` hat keine Standardeingabe. Solange eine dieser Aktion zugeordnete Taste oder Maustaste gehalten wird, neigen die Stick-Eingaben der Tastatur den Stick um `StickModifierScale` seines Bereichs statt voll, zum Gehen oder feinen Zielen. Controller-Sticks und der Maus-Stick sind nicht betroffen, und entgegengesetzte Tasten heben sich weiterhin auf.

```ini
StickModifier = KEY:Z
StickModifierScale = 0.4
```

Mit `KeyboardRampMs` über 0 bewegen Stick-Eingaben der Tastatur den Stick mit gleichmäßiger Geschwindigkeit zu ihrer Neigung, statt zu springen, und ebenso zurück zur Mitte, wenn die Tasten losgelassen werden. Gleichzeitig gehaltene entgegengesetzte Tasten bewegen den Stick mit derselben Geschwindigkeit zur Mitte.

Mit `TriggerRampMs` über 0 drückt eine `L2` oder `R2` zugeordnete Taste oder Maustaste den analogen Trigger allmählich und lässt ihn ebenso los, für Gas und Bremse in Rennspielen. Ein gedrückter Controller-Trigger wird nicht verlangsamt, und der größere der beiden Werte wird weitergegeben.

## Umschaltbare Tasten

Eine in `ToggleActions` aufgeführte Pad-Taste rastet ein, statt gehalten zu werden: Der erste Druck einer ihr zugeordneten Taste oder Maustaste drückt die Pad-Taste, und sie bleibt gedrückt, nachdem die Taste oder Maustaste losgelassen wurde. Der nächste Druck löst sie. Alle derselben Aktion zugeordneten Tasten und Maustasten teilen diesen einen Zustand, sodass ein Druck auf eine beliebige davon ihn umschaltet. Nicht aufgeführte Pad-Tasten behalten das normale Verhalten und sind gedrückt, solange die Taste gehalten wird. Bei Fokusverlust werden alle umgeschalteten Pad-Tasten gelöst.

Mausrad-Eingaben werden nicht umgeschaltet; sie drücken die Pad-Taste wie bisher für einen kurzen Moment.

```ini
ToggleActions = R2, L1
R2 = KEY:Z
```

Hier schaltet Z beim ersten Druck R2 ein und beim zweiten aus, und Q (standardmäßig L1 zugeordnet) tut dasselbe für L1. Eine aufgeführte Aktion ohne Eingabe wird ignoriert. Ein Profil pro Spiel, das `ToggleActions` setzt, ersetzt die Liste aus der gemeinsamen Datei, und ein leerer Wert oder eine unbekannte Aktion ist ein Fehler.

## Schütteln

Solange die Eingabe `Shake` gehalten wird (standardmäßig X), meldet der Controller alle 50 ms eine hin und her schwingende Beschleunigung und Winkelgeschwindigkeit, für Titel, die den Spieler auffordern, den Controller zu schütteln. Andernfalls werden die eigenen Bewegungssensoren eines Controllers verwendet.

## Profile pro Spiel

Eine Datei namens `anyps5-input.<TITLEID>.ini` neben `anyps5-input.ini` (oder neben der Datei, die `ANYPS5_INPUT_CONFIG` nennt) wird danach gelesen, wenn der laufende Titel diese ID hat, zum Beispiel `anyps5-input.PPSA00001.ini`. Ist `ANYPS5_INPUT_CONFIG` nicht gesetzt, kannst du das Profil neben der Spielprogrammdatei verwenden, ohne `anyps5-input.ini` anzulegen. Das Profil hat dasselbe Format und überschreibt nur die Aktionen, Einstellungen und SDL-Controller-Tasten, die es aufführt. Andere Titel lesen es nicht. Eine Title-ID darf Buchstaben, Ziffern, `_` und `-` enthalten und ist höchstens 32 Zeichen lang (`MaxTitleIdLength`). Eine längere ID oder eine mit anderen Zeichen wählt keine Datei pro Spiel aus, und dazu wird keine Meldung protokolliert. Eine ungültige Zeile in einem Profil wird mit Datei und Zeilennummer des Profils gemeldet.

Zum Beispiel ein Profil, das einem Titel eine niedrigere Mausempfindlichkeit und Cross auf F gibt:

```ini
MouseSensitivity = 0.6
Cross = KEY:F
```

## Maus als Analogstick

`ToggleMouse` (standardmäßig die mittlere Maustaste oder F7; eine Maus ohne mittlere Taste kann F7 verwenden) erfasst die Maus und steuert den mit `MouseStickTarget` gewählten Analogstick (standardmäßig `Right`) danach, wie schnell sich die Maus bewegt. Die Erfassung verwendet rohe relative Eingaben, daher gelten die im Betriebssystem eingestellte Zeigergeschwindigkeit und -beschleunigung nicht. Drücke die Taste erneut, um die Maus freizugeben. Die Erfassung endet außerdem, und alle gehaltenen Tasten werden losgelassen, wenn das Fenster den Fokus verliert.

Der Stick neigt sich in die Richtung, in die sich die Maus bewegt, und umso weiter, je schneller sie sich bewegt. Die Geschwindigkeit wird in Counts pro Sekunde über die Zeit zwischen Aktualisierungen gemessen, sodass dieselbe Handgeschwindigkeit bei jeder Bildrate dieselbe Neigung ergibt. Jede Bewegung neigt den Stick mindestens um `MouseDeadzoneOffset`, damit langsame Bewegung nicht in der Deadzone des Spiels verloren geht. Hält die Maus an, kehrt der Stick allmählich zurück und ist `MouseDecayMs` nach der letzten Bewegung zentriert.

So steuerst du den linken Analogstick mit Mausbewegung:

```ini
MouseStickTarget = Left
```

Zum Beispiel für ein Spiel mit kleiner Deadzone, das schnelle Drehungen braucht:

```ini
MouseSensitivity = 1.6
MouseDeadzoneOffset = 0.12
MouseCurve = 1.3
```

Frühere Versionen haben die Maus alle 33 ms abgetastet und den Stick bei jeder Bewegung mindestens zur Hälfte geneigt. `MouseDeadzoneOffset = 0.5`, `MouseSmoothingMs = 0` und `MouseDecayMs = 33` kommen diesem Verhalten nahe.

Eine ungültige Zeile beendet das Spiel beim Start der Videoausgabe mit einem Fehler, der Datei und Zeilennummer nennt. Existiert die von `ANYPS5_INPUT_CONFIG` genannte Datei nicht oder kann die Konfigurationsdatei nicht gelesen werden, wird das Spiel mit einem Fehler beendet.

## Web-Konfigurator

Der [Eingabe-Mapper](https://boykopovar.github.io/AnyPS5/input-mapper/) bearbeitet Eingaben für Tastatur, Maus, Mausrad und Controller-Kombinationen im Browser. Er unterstützt alternative Eingaben, Erfassen physischer Tasten, Suche, INI-Import, Vorschau, Kopieren und Download. Suche nach Aktions- oder Gruppennamen, nach Tastennamen wie `F7` oder nach einer vollständigen Eingabe wie `MOUSE:Left`. Die Suche nach Eingaben umfasst die konfigurierten Eingaben und die Standardeingaben von Aktionen ohne Überschreibung. Ungültige Importe lassen die aktuelle Konfiguration unverändert. Ungültige Eingaben blockieren den Export; doppelte und geteilte Eingaben, einschließlich beibehaltener Standardeingaben, erzeugen Warnungen. Werden alle Eingaben einer Aktion entfernt, werden ihre Standardeingaben wiederhergestellt. Der gehostete Mapper folgt `main`, daher erfordern Einstellungen, die seit einem Release hinzugekommen sind, einen neueren Build.

Für eine Emulatoraktion wie `ToggleFullscreen` füge eine Eingabe hinzu, wähle `PAD` als Eingabetyp und wähle im Eingabewert eine oder mehrere Controller-Tasten. Öffne eine importierte `PAD:`-Eingabe auf dieselbe Weise, um ihre Tasten zu bearbeiten.

Gib auf der Seite Konfiguration die exakte Title-ID des Spiels ein, um ein [Profil pro Spiel](#profile-pro-spiel) wie `anyps5-input.PPSA00001.ini` herunterzuladen. Lass das Feld leer, um die gemeinsame `anyps5-input.ini` herunterzuladen. Der Import von `anyps5-input.<TITLEID>.ini` füllt das Feld aus dem Dateinamen. Der Import von `anyps5-input.ini` leert es.

Öffne auf der Seite Konfiguration **Controller-Tasten**, um eine SDL-Taste einer PlayStation-Taste zuzuordnen. Wähle `None`, um sie zu deaktivieren, oder **Keine eigene Belegung**, um ihre `SDLButton:`-Zeile wegzulassen und die geerbte Zuordnung zu behalten. Diese Zuordnungen gelten für alle verbundenen Controller.

Bearbeite unter Eingabeeinstellungen Werte wie `MouseSensitivity` oder `ToggleActions`. Lass eine Einstellung leer, um ihren Wert zu übernehmen. Der Konfigurator prüft Einstellungen und Controller-Zuordnungen und übernimmt sie in Vorschau, Zwischenablage und heruntergeladene Datei. Ungültige oder doppelte Einträge werden mit ihrer Zeilennummer gemeldet.

Im Stammordner des Repositorys, mit Node.js 22 oder neuer:

```sh
npm --prefix tools/input-mapper start
```

Öffne `http://127.0.0.1:4173`. Beende den Server mit Strg+C. Es ist keine Paketinstallation nötig. Setze `PORT`, um einen anderen Port zu verwenden. Führe die Tests aus mit:

```sh
npm --prefix tools/input-mapper test
```

Steuerung, Konfiguration und Hilfe sind getrennte Ansichten; beim Wechsel der Ansicht bleiben die Eingaben erhalten. Die Suche umfasst alle Steuerungsgruppen. Die Gruppenauswahl unterstützt Pfeiltasten, Pos1 und Ende. Eingabe- und Sprachmenüs unterstützen Suche, Pfeiltasten und Enter; Escape bricht die Auswahl ab. Menüs öffnen sich auf größeren Bildschirmen neben ihren Feldern und auf Smartphones als Bereich am unteren Bildschirmrand. Die Oberfläche verwendet Systemschriften und berücksichtigt die Einstellung für reduzierte Bewegung.

Die Übersetzungsschaltfläche wählt eine von 24 Sprachen für Oberfläche und FAQ. Beim ersten Besuch wird Englisch verwendet; eine gespeicherte Sprachwahl hat Vorrang. Ägyptisches Arabisch steht in der Sprachliste an erster Stelle. Arabisch, Urdu und Persisch verwenden Layouts von rechts nach links. Nur die Spracheinstellung wird gespeichert; kopiere Änderungen an den Eingaben oder lade sie herunter, bevor du neu lädst.

Die Tastaturerfassung wandelt physische Tastencodes des Browsers in SDL-Scancode-Namen um. Escape bricht die Erfassung ab; wähle Escape manuell aus, um es zuzuweisen. Komposition, mehrdeutige Tasten und manche Browser- oder Systemkürzel erfordern eine manuelle Auswahl. Namen, die `#` oder `;` enthalten, können nicht exportiert werden, weil diese Zeichen INI-Kommentare einleiten. Der Tastenkatalog folgt dem SDL-Commit `74be4c2836da0940f0144d0d97852606f9ab11e9`, auf den `3rdparty/SDL2` festgelegt ist. Die Source-Contract-Tests vergleichen Aktionen, Einschränkungen und Standardwerte mit dem aktuellen nativen Eingabecode.

Für statisches Hosting stelle `index.html`, `styles.css`, `app.js`, `mapping.js`, `keys.js`, `capture.js`, `validation.js`, `i18n.js` und `dropdown.js` aus einem Ordner bereit.

## Zeitleiste ohne Controller prüfen

- `tools/preview_pad_timeline.py <timeline> [--repeat N] [--gap-ms MS] [--json]` prüft eine Controller-Zeitleiste und zeigt eine Vorschau, ohne eine Controller-Befehlsdatei anzufassen. `--json` gibt die expandierten Ereignisse und ihre Dauer als JSON aus. Die Dauer endet mit dem Loslassen des letzten Ereignisses.
  Mit `--repeat` gibt `--gap-ms` die Verzögerung vom letzten Loslassen eines Durchlaufs bis zum Zeitpunkt null des nächsten Durchlaufs an.
- `tools/preview_pad_input.py <path> <button> [--hold-ms MS] [--left-stick X Y] [--right-stick X Y] [--touch X Y ID ACTIVE]` gibt einen einzelnen Controller-Impuls aus, ohne die Befehlsdatei unter `<path>` zu ändern.
