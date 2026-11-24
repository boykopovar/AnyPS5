# Controller, keyboard and mouse input

AnyPS5 accepts SDL-mapped game controllers as well as its built-in keyboard and mouse bindings. Up to 4 controllers are supported simultaneously. By default, controllers fill Players 1 through 4 in discovery order (matching `libSceUserService` users). Only the controller in the Player 1 slot is combined with keyboard and mouse input. See [stable player assignment](#stable-player-assignment) to reserve slots by serial. Xbox-style controllers and PlayStation controllers recognized by SDL use the standard PS button layout; sticks and analog triggers are passed through, and controllers can be connected or disconnected dynamically while the game is running. AnyPS5 uses its built-in input bindings when no configuration file is present. To change input bindings, create `anyps5-input.ini` beside the generated game executable. Set `ANYPS5_INPUT_CONFIG` to use a file at another path. Both the game directory and that path may contain any characters, such as `ü` or `日本`. With `ANYPS5_PAD_PORT0_STATE=controller`, keyboard and mouse input reaches the game only while a controller is bound to Player 1; see [Controller connection and user 0](USAGE.md#controller-connection-and-user-0).

For an Xbox-layout controller, A/B/X/Y map to Cross/Circle/Square/Triangle, LB/RB map to L1/R1, and LT/RT map to analog L2/R2. Start maps to Options. D-pad and stick-click buttons are supported. On a PlayStation controller, touchpad clicks and the positions of up to two fingers on the touchpad are forwarded. On other controllers, View (Back) presses the touchpad at its center.

Keyboard, mouse, wheel and hotkey bindings use the form `Action = Type:Value`. Action names are case-insensitive. A `#` or `;` starts a comment. The first line for an action replaces its built-in bindings; later lines for the same action add alternate inputs. Duplicate entries for the same action are ignored. Actions omitted from the file keep their built-in bindings. UTF-8 files with or without a byte-order mark are supported.

Supported input sources are:

- `KEY:Return`, `KEY:Space`, or another key name accepted by SDL.
- `MOUSE:Left`, `MOUSE:Middle`, `MOUSE:Right`, `MOUSE:X1`, or `MOUSE:X2`.
- `WHEEL:Up`, `WHEEL:Down`, `WHEEL:Left` or `WHEEL:Right` for pad buttons. `Left` and `Right` are horizontal scroll steps, such as a tilting wheel's sideways clicks; no action uses them by default.
- `PAD:Button` or `PAD:Button+Button+...` for emulator actions only: a chord of controller buttons that are all held at once. Button names are `Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left` and `TouchPad`. The chord fires once, when its last button goes down, and the buttons still reach the title. `PAD:` is accepted for `ToggleFullscreen`, `TogglePerformanceOverlay`, `Screenshot`, `ShowPadStatus`, `VolumeUp`, `VolumeDown`, `ToggleMute` and `ReloadInputMapping`; other actions refuse it. Example: `ToggleFullscreen = PAD:Options+R1`.

While a title has a mouse open (`sceMouseOpen`) and the window is focused, AnyPS5 hides and captures the host cursor, so the title's own cursor is the only one; the capture ends when the window loses focus or when the mouse-to-stick toggle (`ToggleMouse`) is on. While the title has the mouse, mouse clicks and the wheel go to the title instead of their pad bindings. Turning on `ToggleMouse` with its key (F7 by default) hands the whole mouse to the pad until it is turned off again: motion moves the stick, and clicks and the wheel press their pad bindings, while the title's mouse reports no movement, clicks or wheel turns. A title that opens the mouse but ignores it in play can be played this way. Mouse movement reaches the title scaled from the window's presented image to the title's display resolution, so the title's cursor moves as far as the host cursor would.

The window hotkeys (`ToggleFullscreen`, `TogglePerformanceOverlay`, `Screenshot`, `ShowPadStatus`, `VolumeDown`, `VolumeUp` and `ToggleMute`) accept keyboard keys and `PAD:` chords. The buttons in a chord still reach the title when the hotkey fires. A key bound to a hotkey and to another action triggers both, as for every other action; the built-in keys of the actions above share none.

Game-controller bindings are enabled automatically. Keyboard and mouse overrides apply independently of [controller button remapping](#controller-button-remapping).

Supported actions are `Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left`, `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown`, `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown`, `TouchLeft`, `TouchRight`, `StickModifier`, `Shake`, `ToggleMouse`, `ToggleFullscreen` (F11), `TogglePerformanceOverlay` (F10, the [performance overlay](USAGE.md#performance-overlay)), `Screenshot` (F12, saves the next displayed frame as described in [screenshots](USAGE.md#screenshots)), and `ShowPadStatus` (F9, shows the [controller status panel](USAGE.md#controller-status-panel) for a few seconds). `ToggleFullscreen`, `TogglePerformanceOverlay`, `Screenshot` and `ShowPadStatus` also accept `PAD:` chords.

### SDL controller detection and layout

If SDL sees a joystick but the game shows `NO CONTROLLER`, or the D-pad or buttons report the wrong inputs, supply an SDL2 mapping for the controller's GUID and operating system. USB and Bluetooth can use different mappings. Download `gamecontrollerdb.txt` from [SDL GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB), or save the mapping line from SDL2's `controllermap` tool in a plain text file, including its `platform:` field.

With `gamecontrollerdb.txt` beside the converted game, start the game from that directory. On Windows, the setting applies to programs launched from the same PowerShell session until it closes.

```powershell
$env:SDL_GAMECONTROLLERCONFIG_FILE = (Resolve-Path -LiteralPath '.\gamecontrollerdb.txt').Path
.\app.exe
```

On Linux, set the variable for the game process.

```sh
SDL_GAMECONTROLLERCONFIG_FILE="$PWD/gamecontrollerdb.txt" ./app.elf
```

Press F9 to check the [controller status panel](USAGE.md#controller-status-panel). SDL loads the file at startup. A matching file entry overrides SDL's built-in mapping, and a mapping set directly in `SDL_GAMECONTROLLERCONFIG` takes priority over the file. The `SDLButton:` entries in `anyps5-input.ini` then assign the resulting SDL buttons to PlayStation buttons.

### Controller button remapping

Use `SDLButton:button = Button` to assign an SDL controller button to a PlayStation button. For example, this swaps Cross and Circle, then disables the Back button.

```ini
SDLButton:a = Circle
SDLButton:b = Cross
SDLButton:back = None
```

Each SDL button can appear once per file. An omitted button keeps its default or its shared-file assignment. These overrides apply to all controller slots. SDL button names are case-insensitive and include `a`, `b`, `x`, `y`, `back`, `start`, `leftshoulder`, `rightshoulder`, `leftstick`, `rightstick`, `dpup`, `dpdown`, `dpleft`, `dpright`, `guide`, `misc1`, `paddle1` through `paddle4`, and `touchpad`, depending on the device. The destination accepts the same button names as `PAD:` chords or `None`. Mapping a button to `L2` or `R2` produces a full trigger press. Sticks and physical analog triggers keep their usual behavior. `PAD:` hotkeys follow the resulting PlayStation button assignments.

### Reloading the mapping

`ReloadInputMapping` (F1 by default) reads `anyps5-input.ini` and the title's profile again while the game keeps running, so you can try a change without restarting the title. The new file is read completely before anything changes: if it has an error, the previous mapping stays active and the log names the file, the line and the reason. A successful reload replaces the whole mapping at once.

Set `ANYPS5_INPUT_WATCH=1` to reload automatically when the file or the title's profile changes. AnyPS5 checks the modification time and size about once a second on a background thread, and reloads only after a change has stayed the same for one check, so a file still being saved is not read half-written. The option is off by default.

A key bound to `ShowPadStatus` and to another action triggers both, as for every other action. F9 is the built-in key of `ShowPadStatus`: if you moved `Screenshot`, `TogglePerformanceOverlay` or another action to F9, also rebind `ShowPadStatus` to a key you do not use, or each press shows the controller status panel too. The [web configurator](#web-configurator) warns about a key that two actions share.

Three more actions control the host's output volume: `VolumeDown` (F5), `VolumeUp` (F6) and `ToggleMute` (F8), described in [host volume and mute](USAGE.md#host-volume-and-mute). They also accept `PAD:` chords.

### Default bindings

Each action keeps its built-in bindings unless the shared file or the current title's [per-game profile](#per-game-profiles) overrides it.

| Action | Keyboard and mouse |
|--------|--------------------|
| `Cross` | Return, Space |
| `Circle` | C |
| `Triangle` | I |
| `Square` | Left mouse button |
| `L1` | Q |
| `R1` | E, Left Alt, Right Alt |
| `L2` | R, Mouse button 4 (X1) |
| `R2` | Right mouse button |
| `L3` | Left Shift, Right Shift |
| `R3` | Left Ctrl, Right Ctrl |
| `Options` | Escape |
| `Up`, `Right`, `Down`, `Left` | Arrow keys; the mouse wheel also presses `Up` (wheel up) and `Down` (wheel down) |
| `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown` | A, D, W, S |
| `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown` | F, H, T, G |
| `TouchLeft`, `TouchRight` | Backspace, Tab |
| `Shake` | X (reports a swinging acceleration and angular velocity while held) |
| `ToggleMouse` | Middle mouse button, F7 |
| `ToggleFullscreen` | F11 |
| `TogglePerformanceOverlay` | F10 |
| `Screenshot` | F12 |
| `ShowPadStatus` | F9 |
| `VolumeDown` | F5 |
| `VolumeUp` | F6 |
| `ToggleMute` | F8 |
| `ReloadInputMapping` | F1 |

For example, this changes Cross to F or Space, moves the left stick to IJKL, uses the mouse buttons for Square and R2, makes the middle mouse button the only `ToggleMouse` input (the line replaces F7), and keeps all other built-in bindings:

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

The same file also accepts these settings, each at most once, in the form `Setting = Value`:

| Setting | Value | Default |
|---------|-------|---------|
| `LeftStickDeadzone`, `RightStickDeadzone` | Whole percentage from 0 to 90 of the controller stick's travel that reads as centered. Outside it the travel is rescaled, so the stick still reaches its full range. Keyboard and mouse stick bindings are not affected. | `0` |
| `MouseSensitivity` | Number greater than 0 and at most 10. At 1, moving the mouse 3000 counts per second tilts the stick fully, about 5 cm per second at 1600 DPI; a larger number needs less movement. | `1` |
| `MouseStickTarget` | `Left` or `Right`. Selects which analog stick is controlled by mouse movement when `ToggleMouse` is enabled. | `Right` |
| `MouseVerticalScale` | Number from 0.1 to 10 that multiplies vertical mouse speed, for games that pitch slower or faster than they turn. | `1` |
| `MouseCurve` | Number from 0.2 to 5, the response exponent. Above 1 gives finer control at low speed; below 1 reaches large tilts sooner. | `1` |
| `MouseDeadzoneOffset` | Number from 0 to 0.9, the smallest tilt any movement produces as a fraction of full tilt. Set it just above the game's own deadzone: raise it if slow movement doesn't move the camera, lower it if small movements jump. | `0.2` |
| `MouseMaxSpeed` | Number from 0.1 to 10, the highest tracked speed as a multiple of the full-tilt speed. Above 1, a fast flick keeps the stick at full tilt a little longer after the mouse stops. | `1` |
| `MouseSmoothingMs` | Number from 0 to 500, the smoothing time constant in milliseconds. A mouse that reports fewer than 300 times per second, such as a 125 or 250 Hz mouse, is smoothed over at least three of its report intervals, up to 30 ms; the report rate is measured from the movement itself. 0 follows every mouse report directly and turns this off too. | `10` |
| `MouseDecayMs` | Number from 1 to 1000, the milliseconds without movement after which the stick is centered. | `30` |
| `StickModifierScale` | Number from 0.05 to 1, the tilt of keyboard stick bindings while a `StickModifier` binding is held. | `0.5` |
| `KeyboardRampMs` | Number from 0 to 2000, the milliseconds a keyboard stick binding takes to move the stick from the center to full tilt. 0 tilts at once. | `0` |
| `TriggerRampMs` | Number from 0 to 2000, the milliseconds a key or mouse button bound to `L2` or `R2` takes to press the trigger from 0 to full. 0 presses it fully at once. | `0` |
| `ToggleActions` | Comma-separated pad buttons (`Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left`), each toggled by the keys and mouse buttons bound to it. Stick directions and emulator actions are refused. See [Toggled buttons](#toggled-buttons). | none |

```ini
LeftStickDeadzone = 12
RightStickDeadzone = 8
MouseSensitivity = 2.5
```

## Controller timelines

Set `APS5_PAD_INPUT_FILE` as described in [Controller input for test runs](USAGE.md#controller-input-for-test-runs), then queue an entire button sequence after the application begins reading its controller. Save a JSON file such as `menu.json`:

```json
{"version":1,"events":[{"at_ms":200,"hold_ms":300,"buttons":["cross","r1"]},{"at_ms":1000,"hold_ms":300,"buttons":["down"]}]}
```

```sh
python3 tools/pad_timeline.py menu.json /tmp/anyps5-pad --repeat 3 --gap-ms 500
```

Events start relative to when the application first observes the new command. Button lists support every name in `pad_input.py`; simultaneous names are combined. An empty list or `["release"]` injects no buttons. An event may also hold `left_stick` and `right_stick` positions as `[x, y]` from 0 to 255 (128 is centred); a stick an event names but the other leaves out stays centred for that event, and an event without either keeps the physical sticks, for example `{"at_ms":0,"hold_ms":400,"buttons":[],"left_stick":[255,128]}` to push the left stick right. Events must be ordered and non-overlapping, hold for 1–5000 ms, and finish within one hour. Repetition adds the specified gap after the last event; at most 4096 events are accepted after repetition, and the command file they produce must not exceed the 64 KiB the runtime reads. The helper validates the entire sequence before atomically replacing the command file.

The runtime evaluates scheduled events at each controller read, including their automatic releases. A new pulse or timeline replaces the remaining sequence. Physical controller buttons still add to injected buttons. Existing commands at the application's first controller read are ignored. This schedules wall-time input: events can be missed while the application is not reading its controller, and it does not reproduce frame timing or game state. Use gaps and holds long enough for the title's input loop.

### Automated title runs

`tools/title_run.py` queues a title timeline only after both the `after_s` minimum delay and a fresh recording sample from controller port 0. This keeps a timeline queued during startup from being discarded as an old command at the first controller read. For scripted input, an absent or empty `APS5_PAD_RECORD` defaults to `pad-record.txt` in the run trace directory. Nonempty overrides are preserved; relative paths resolve in the title work directory. Warmup and capture each wait for a new sample, and `pad_queued_s` stays null if the process ends before readiness.

## Recording and replaying controller input

Set `APS5_PAD_RECORD` to a file path to record what a title reads from controller port 0 while you play: every change of its buttons, sticks and analog triggers, whatever device produced it, timed in milliseconds from the title's first controller read. Set `APS5_PAD_REPLAY` to that file on a later run to feed the same states back at the same times from that run's first read, without any device. A replay replaces port 0's buttons, sticks and analog triggers at every read; after the last recorded change it keeps the last state. A recording path that cannot be written, or a replay file that cannot be read or is not in the format below, stops the title at its first controller read with an error naming the variable.

```sh
APS5_PAD_RECORD=/tmp/fight.txt ./app.elf
APS5_PAD_REPLAY=/tmp/fight.txt APS5_NO_PAD_INPUT=1 ./app.elf
```

The file starts with the line `APS5_PAD_RECORDING 1`, followed by one line per change: the time in milliseconds, the buttons in hexadecimal (the values of `pad_input.py`), the left and right stick x and y, and L2 and R2, each 0 to 255. A replay follows the recorded times, not the title's frames: a run that loads more slowly or runs at another frame rate, or a title that draws different random numbers, drifts from the recorded session, so compare runs by scene rather than by second.

## Controller vibration strength

Vibration is not part of the input mapping file. The host-side strength of a controller's vibration is set with `ANYPS5_RUMBLE`, described in [Controller vibration strength](USAGE.md#controller-vibration-strength); `0` turns it off for every connected controller. Vibration also pauses while the window has no focus unless `ANYPS5_RUMBLE_ON_FOCUS_LOSS=0` is set, see [Vibration while the window has no focus](USAGE.md#vibration-while-the-window-has-no-focus).

The identification pulse that tells players' controllers apart is set with `ANYPS5_PAD_IDENTIFY`, described in [Controller identification pulse](USAGE.md#controller-identification-pulse).

## Stable player assignment

By default, connected controllers fill the first free player slot in SDL's discovery order. To keep a controller on the same player after reconnecting or restarting, set `ANYPS5_PAD_PLAYER_1_SERIAL` through `ANYPS5_PAD_PLAYER_4_SERIAL` to the exact SDL serial for each player. Set `ANYPS5_PAD_LOG_CONTROLLER_SERIAL=1` to include serials in the connection log while configuring this; serial logging is off by default. SDL may not expose a serial for every controller or backend. Such controllers, and controllers whose serial is not configured, use the first free unreserved player slot. A configured slot stays reserved while its controller is disconnected, so other controllers do not take that player's place. Assigning the same serial to multiple players stops startup with an error.

For example, to bind two controllers and leave the other player slots automatic:

```sh
ANYPS5_PAD_PLAYER_1_SERIAL=ABC123 ANYPS5_PAD_PLAYER_2_SERIAL=XYZ789 ./app.elf
```

Serials may differ when a controller is connected through another transport or exposed by a different SDL backend. Check the connection log for the exact value reported by the current host.

## Analog keyboard input

`StickModifier` has no built-in binding. While a key or mouse button bound to it is held, the keyboard stick bindings tilt the stick by `StickModifierScale` of its range instead of fully, for walking or fine aim. Controller sticks and the mouse stick are not affected, and opposite keys still cancel each other.

```ini
StickModifier = KEY:Z
StickModifierScale = 0.4
```

With `KeyboardRampMs` above 0, keyboard stick bindings move the stick toward their tilt at a steady speed instead of jumping, and back to the center the same way when the keys are let go. Opposite keys held together move the stick to the center at the same speed.

With `TriggerRampMs` above 0, a key or mouse button bound to `L2` or `R2` presses the analog trigger gradually and lets it go the same way, for throttle and brake in racing titles. A pressed controller trigger is not slowed, and the larger of the two values is passed on.

## Toggled buttons

A pad button listed in `ToggleActions` latches instead of being held: the first press of a key or mouse button bound to it presses the button, and the button stays pressed after the key or mouse button is released. The next press releases it. Every key and mouse button bound to the same action shares that one state, so pressing either one toggles it. Buttons not listed keep the normal behavior, pressed while the key is held. Losing focus releases every toggled button.

Mouse wheel bindings are not toggled; they still press the button for a short moment as before.

```ini
ToggleActions = R2, L1
R2 = KEY:Z
```

Here Z presses R2 on the first press and off on the second, and Q (bound to L1 by default) does the same for L1. A listed action with no binding is ignored. A per-game profile that sets `ToggleActions` replaces the list from the shared file, and an empty value or an unknown action is an error.

## Shake

While the `Shake` input is held (X by default), the controller reports its acceleration and angular velocity swinging back and forth every 50 ms, for titles that ask the player to shake the controller. A controller's own motion sensors are used otherwise.

## Per-game profiles

A file named `anyps5-input.<TITLEID>.ini` beside `anyps5-input.ini` (or beside the file `ANYPS5_INPUT_CONFIG` names) is read after it when the running title has that ID, for example `anyps5-input.PPSA00001.ini`. With `ANYPS5_INPUT_CONFIG` unset, you can use the profile beside the game executable without creating `anyps5-input.ini`. The profile has the same format and overrides only the actions, settings and SDL controller buttons it lists. Other titles don't read it. A title ID may contain letters, digits, `_` and `-`. An invalid line in a profile reports the profile's file and line number.

For example, a profile that gives one title a lower mouse sensitivity and Cross on F:

```ini
MouseSensitivity = 0.6
Cross = KEY:F
```

## Mouse as an analog stick

`ToggleMouse` (the middle mouse button or F7 by default; a mouse without a middle button can use F7) captures the mouse and drives the analog stick selected by `MouseStickTarget` (`Right` by default) from how fast the mouse moves. Capture uses raw relative input, so the pointer speed and acceleration set in the operating system don't apply. Press it again to release the mouse. Capture also ends, and every held key is let go, when the window loses focus.

The stick tilts in the direction the mouse moves, and further the faster it moves. Speed is measured in counts per second over the time between updates, so the same hand speed gives the same tilt at any frame rate. Any movement tilts the stick at least `MouseDeadzoneOffset`, so slow movement isn't lost inside the game's own deadzone. When the mouse stops, the stick eases back and is centered `MouseDecayMs` after the last movement.

To control the left analog stick with mouse movement:

```ini
MouseStickTarget = Left
```

For example, for a game with a small deadzone that needs fast turns:

```ini
MouseSensitivity = 1.6
MouseDeadzoneOffset = 0.12
MouseCurve = 1.3
```

Earlier versions sampled the mouse every 33 ms and tilted the stick at least halfway for any movement. `MouseDeadzoneOffset = 0.5`, `MouseSmoothingMs = 0` and `MouseDecayMs = 33` come close to that behavior.

An invalid line stops the game when video output starts, with an error that names the file and line number. If the file named by `ANYPS5_INPUT_CONFIG` does not exist, or the configuration file cannot be read, the game stops with an error.

## Web configurator

The [input mapper](../../tools/input-mapper/index.html) edits keyboard, mouse, wheel and controller chord bindings in the browser. It supports alternate bindings, physical-key capture, search, INI import, preview, copy and download. Search by action or group name, by key name such as `F7`, or by full input such as `MOUSE:Left`. Input searches include the configured bindings and the built-in bindings for actions without overrides. Invalid imports preserve the current configuration. Invalid bindings block export; duplicate and shared inputs, including retained built-in bindings, produce warnings. Removing every binding for an action restores its built-in inputs.

For an emulator action such as `ToggleFullscreen`, add a binding, choose `PAD` as the input type and select one or more controller buttons in the binding value. Open an imported `PAD:` binding the same way to edit its buttons.

On the Configuration page, enter the game's exact title ID to download a [per-game profile](#per-game-profiles), such as `anyps5-input.PPSA00001.ini`. Leave the field blank to download the shared `anyps5-input.ini`. Importing `anyps5-input.<TITLEID>.ini` fills the field from the filename. Importing `anyps5-input.ini` clears it.

On the Configuration page, open **Controller buttons** to assign an SDL button to a PlayStation button. Choose `None` to disable it or **No override** to omit its `SDLButton:` line and keep the inherited mapping. These assignments apply to all connected controllers.

Add settings such as `MouseSensitivity` or `ToggleActions` by hand. The configurator validates settings and controller assignments on import and preserves them in the preview, clipboard and downloaded file. Invalid or duplicate entries are reported with their line number.

From the repository root, with Node.js 22 or later:

```sh
npm --prefix tools/input-mapper start
```

Open `http://127.0.0.1:4173`. Stop the server with Ctrl+C. No package installation is required. Set `PORT` to use another port. Run the tests with:

```sh
npm --prefix tools/input-mapper test
```

Controls, Configuration and Help are separate views; switching views preserves bindings. Search covers all control groups. The group selector supports arrow keys, Home and End. Input and language menus support search, arrow keys and Enter; Escape cancels selection. Menus open beside their fields on larger screens and as bottom sheets on phones. The interface uses system fonts and respects reduced-motion preferences.

The translation button selects one of 22 interface and FAQ languages. First visits use English; a saved language choice takes priority. Egyptian Arabic appears first in the language list. Arabic, Urdu and Persian use right-to-left layouts. Only the language preference is persisted; copy or download input changes before reloading.

Keyboard capture converts physical browser key codes to SDL scancode names. Escape cancels capture; select Escape manually to assign it. Composition, ambiguous keys and some browser or system shortcuts require manual selection. Names containing `#` or `;` cannot be exported because those characters start INI comments. The key catalogue follows SDL commit `74be4c2836da0940f0144d0d97852606f9ab11e9`, the commit `3rdparty/SDL2` is pinned to. The source-contract tests compare actions, restrictions and defaults with the current native input code.

For static hosting, serve `index.html`, `styles.css`, `app.js`, `mapping.js`, `keys.js`, `capture.js`, `validation.js`, `i18n.js` and `dropdown.js` from one directory. The configurator is not published on the project's GitHub Pages site yet.

## Checking a timeline without a controller

- `tools/preview_pad_timeline.py <timeline> [--repeat N] [--gap-ms MS] [--json]` validates a controller timeline and previews it without touching a controller command file. `--json` prints the preview as JSON.
- `tools/preview_pad_input.py <path> <button> [--hold-ms MS] [--left-stick X Y] [--right-stick X Y] [--touch X Y ID ACTIVE]` prints one controller pulse without modifying the command file at `<path>`.
