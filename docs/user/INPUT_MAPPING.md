# Controller, keyboard and mouse input

AnyPS5 accepts SDL-mapped game controllers as well as its built-in keyboard and mouse bindings. Xbox-style controllers and PlayStation controllers recognized by SDL use the standard PS button layout; sticks and analog triggers are passed through, and controllers can be connected or disconnected while the game is running. The first recognized controller is used. AnyPS5 uses its built-in keyboard and mouse bindings when no configuration file is present. To change selected keyboard and mouse bindings, create `anyps5-input.ini` beside the generated game executable. Set `ANYPS5_INPUT_CONFIG` to use a file at another path. Both the game directory and that path may contain any characters, such as `ü` or `日本`.

For an Xbox-layout controller, A/B/X/Y map to Cross/Circle/Square/Triangle, LB/RB map to L1/R1, and LT/RT map to analog L2/R2. Start maps to Options. D-pad and stick-click buttons are supported. On a PlayStation controller, touchpad clicks and the positions of up to two fingers on the touchpad are forwarded. On other controllers, View (Back) presses the touchpad at its center.

Each non-empty line has the form `Action = Type:Value`. Action names are case-insensitive. A `#` or `;` starts a comment. The first line for an action replaces its built-in bindings; later lines for the same action add alternate inputs. Duplicate entries for the same action are ignored. Actions omitted from the file keep their built-in bindings. UTF-8 files with or without a byte-order mark are supported.

Supported input sources are:

- `KEY:Return`, `KEY:Space`, or another key name accepted by SDL.
- `MOUSE:Left`, `MOUSE:Middle`, `MOUSE:Right`, `MOUSE:X1`, or `MOUSE:X2`.
- `WHEEL:Up` or `WHEEL:Down` for pad buttons.

While a title has a mouse open (`sceMouseOpen`) and the window is focused, AnyPS5 hides and captures the host cursor, so the title's own cursor is the only one; the capture ends when the window loses focus or when the mouse-to-stick toggle (`ToggleMouse`) is on. Mouse movement reaches the title scaled from the window's presented image to the title's display resolution, so the title's cursor moves as far as the host cursor would.

Game-controller bindings are enabled automatically and are not affected by keyboard or mouse overrides in this file.

Supported actions are `Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left`, `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown`, `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown`, `TouchLeft`, `TouchRight`, `ToggleMouse`, and `ToggleFullscreen`.

### Default bindings

Without an `anyps5-input.ini`, or for actions the file does not mention, these bindings apply:

| Action | Keyboard and mouse |
|--------|--------------------|
| `Cross` | Return, Space |
| `Circle` | C |
| `Triangle` | I |
| `Square` | Left mouse button |
| `L1` | Q |
| `R1` | E, Left Alt, Right Alt |
| `R2` | Right mouse button |
| `L3` | Left Shift, Right Shift |
| `R3` | Left Ctrl, Right Ctrl |
| `Options` | Escape |
| `Up`, `Right`, `Down`, `Left` | Arrow keys; the mouse wheel also presses `Up` (wheel up) and `Down` (wheel down) |
| `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown` | A, D, W, S |
| `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown` | F, H, T, G |
| `TouchLeft`, `TouchRight` | Backspace, Tab |
| `ToggleMouse` | Middle mouse button |
| `ToggleFullscreen` | F11 |

`L2` has no built-in keyboard or mouse binding.

For example, this changes Cross to F or Space, moves the left stick to IJKL, uses the mouse buttons for Square and R2, and keeps all other built-in bindings:

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
| `MouseVerticalScale` | Number from 0.1 to 10 that multiplies vertical mouse speed, for games that pitch slower or faster than they turn. | `1` |
| `MouseCurve` | Number from 0.2 to 5, the response exponent. Above 1 gives finer control at low speed; below 1 reaches large tilts sooner. | `1` |
| `MouseDeadzoneOffset` | Number from 0 to 0.9, the smallest tilt any movement produces as a fraction of full tilt. Set it just above the game's own deadzone: raise it if slow movement doesn't move the camera, lower it if small movements jump. | `0.2` |
| `MouseMaxSpeed` | Number from 0.1 to 10, the highest tracked speed as a multiple of the full-tilt speed. Above 1, a fast flick keeps the stick at full tilt a little longer after the mouse stops. | `1` |
| `MouseSmoothingMs` | Number from 0 to 500, the smoothing time constant in milliseconds. 0 follows every mouse report directly. | `10` |
| `MouseDecayMs` | Number from 1 to 1000, the milliseconds without movement after which the stick is centered. | `30` |

```ini
LeftStickDeadzone = 12
RightStickDeadzone = 8
MouseSensitivity = 2.5
```

## Mouse as the right stick

`ToggleMouse` (the middle mouse button by default) captures the mouse and drives the right stick from how fast the mouse moves. Capture uses raw relative input, so the pointer speed and acceleration set in the operating system don't apply. Press it again to release the mouse. Capture also ends, and every held key is let go, when the window loses focus.

The stick tilts in the direction the mouse moves, and further the faster it moves. Speed is measured in counts per second over the time between updates, so the same hand speed gives the same tilt at any frame rate. Any movement tilts the stick at least `MouseDeadzoneOffset`, so slow movement isn't lost inside the game's own deadzone. When the mouse stops, the stick eases back and is centered `MouseDecayMs` after the last movement.

For example, for a game with a small deadzone that needs fast turns:

```ini
MouseSensitivity = 1.6
MouseDeadzoneOffset = 0.12
MouseCurve = 1.3
```

Earlier versions sampled the mouse every 33 ms and tilted the stick at least halfway for any movement. `MouseDeadzoneOffset = 0.5`, `MouseSmoothingMs = 0` and `MouseDecayMs = 33` come close to that behavior.

An invalid line reports the file and line number and stops input initialization. If the configured file does not exist or cannot be read, AnyPS5 reports an error. With no `anyps5-input.ini` and no `ANYPS5_INPUT_CONFIG`, the built-in mapping is used.

## Web configurator

The [input mapper](../../tools/input-mapper/index.html) edits keyboard, mouse and wheel overrides in the browser. It supports alternate bindings, physical-key capture, search, INI import, preview, copy and download. Invalid imports preserve the current configuration. Invalid bindings block export; duplicate and shared inputs, including retained built-in bindings, produce warnings. Removing every binding for an action restores its built-in inputs.

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

Keyboard capture converts physical browser key codes to SDL scancode names. Escape cancels capture; select Escape manually to assign it. Composition, ambiguous keys and some browser or system shortcuts require manual selection. Names containing `#` or `;` cannot be exported because those characters start INI comments. The key catalogue follows SDL commit `4b69833bc54abf3dd3288d4aa7afbba527775e5b`. The source-contract tests compare actions, restrictions and defaults with the current native input code.

For static hosting, serve `index.html`, `styles.css`, `app.js`, `mapping.js`, `keys.js`, `capture.js`, `validation.js`, `i18n.js` and `dropdown.js` from one directory. The configurator is not published on the project's GitHub Pages site yet.
