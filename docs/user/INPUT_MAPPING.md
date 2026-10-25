# Controller, keyboard and mouse input

AnyPS5 accepts SDL-mapped game controllers as well as its built-in keyboard and mouse bindings. Up to 4 controllers are supported simultaneously. The first connected controller is assigned to Player 1 and composed with keyboard and mouse input; additional controllers are sequentially assigned to Players 2, 3, and 4 (matching `libSceUserService` users). Xbox-style controllers and PlayStation controllers recognized by SDL use the standard PS button layout; sticks and analog triggers are passed through, and controllers can be connected or disconnected dynamically while the game is running. AnyPS5 uses its built-in keyboard and mouse bindings when no configuration file is present. To change selected keyboard and mouse bindings, create `anyps5-input.ini` beside the generated game executable. Set `ANYPS5_INPUT_CONFIG` to use a file at another path. Both the game directory and that path may contain any characters, such as `ü` or `日本`.

For an Xbox-layout controller, A/B/X/Y map to Cross/Circle/Square/Triangle, LB/RB map to L1/R1, and LT/RT map to analog L2/R2. Start maps to Options. D-pad and stick-click buttons are supported. On a PlayStation controller, touchpad clicks and the positions of up to two fingers on the touchpad are forwarded. On other controllers, View (Back) presses the touchpad at its center.

Each non-empty line has the form `Action = Type:Value`. Action names are case-insensitive. A `#` or `;` starts a comment. The first line for an action replaces its built-in bindings; later lines for the same action add alternate inputs. Duplicate entries for the same action are ignored. Actions omitted from the file keep their built-in bindings. UTF-8 files with or without a byte-order mark are supported.

Supported input sources are:

- `KEY:Return`, `KEY:Space`, or another key name accepted by SDL.
- `MOUSE:Left`, `MOUSE:Middle`, `MOUSE:Right`, `MOUSE:X1`, or `MOUSE:X2`.
- `WHEEL:Up` or `WHEEL:Down` for pad buttons.

While a title has a mouse open (`sceMouseOpen`) and the window is focused, AnyPS5 hides and captures the host cursor, so the title's own cursor is the only one; the capture ends when the window loses focus or when the mouse-to-stick toggle (`ToggleMouse`) is on. Mouse movement reaches the title scaled from the window's presented image to the title's display resolution, so the title's cursor moves as far as the host cursor would.

Game-controller bindings are enabled automatically and are not affected by keyboard or mouse overrides in this file.

Supported actions are `Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left`, `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown`, `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown`, `TouchLeft`, `TouchRight`, `StickModifier`, `Shake`, `ToggleMouse`, `ToggleFullscreen` (F11), `TogglePerformanceOverlay` (F10, the [performance overlay](USAGE.md#performance-overlay)), and `Screenshot` (F12, saves the next displayed frame as described in [screenshots](USAGE.md#screenshots)). `ToggleFullscreen`, `TogglePerformanceOverlay` and `Screenshot` accept only keyboard keys.

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
| `MouseSmoothingMs` | Number from 0 to 500, the smoothing time constant in milliseconds. A mouse that reports fewer than 300 times per second, such as a 125 or 250 Hz mouse, is smoothed over at least three of its report intervals, up to 30 ms; the report rate is measured from the movement itself. 0 follows every mouse report directly and turns this off too. | `10` |
| `MouseDecayMs` | Number from 1 to 1000, the milliseconds without movement after which the stick is centered. | `30` |
| `StickModifierScale` | Number from 0.05 to 1, the tilt of keyboard stick bindings while a `StickModifier` binding is held. | `0.5` |
| `KeyboardRampMs` | Number from 0 to 2000, the milliseconds a keyboard stick binding takes to move the stick from the center to full tilt. 0 tilts at once. | `0` |
| `TriggerRampMs` | Number from 0 to 2000, the milliseconds a key or mouse button bound to `L2` or `R2` takes to press the trigger from 0 to full. 0 presses it fully at once. | `0` |

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

Events start relative to when the application first observes the new command. Button lists support every name in `pad_input.py`; simultaneous names are combined. An empty list or `["release"]` injects no buttons. Events must be ordered and non-overlapping, hold for 1–5000 ms, and finish within one hour. Repetition adds the specified gap after the last event; at most 4096 events are accepted after repetition. The helper validates the entire sequence before atomically replacing the command file.

The runtime evaluates scheduled events at each controller read, including their automatic releases. A new pulse or timeline replaces the remaining sequence. Physical controller buttons still add to injected buttons. Existing commands at the application's first controller read are ignored. This schedules wall-time input: events can be missed while the application is not reading its controller, and it does not reproduce frame timing or game state. Use gaps and holds long enough for the title's input loop.

## Analog keyboard input

`StickModifier` has no built-in binding. While a key or mouse button bound to it is held, the keyboard stick bindings tilt the stick by `StickModifierScale` of its range instead of fully, for walking or fine aim. Controller sticks and the mouse stick are not affected, and opposite keys still cancel each other.

```ini
StickModifier = KEY:Z
StickModifierScale = 0.4
```

With `KeyboardRampMs` above 0, keyboard stick bindings move the stick toward their tilt at a steady speed instead of jumping, and back to the center the same way when the keys are let go. Opposite keys held together move the stick to the center at the same speed.

With `TriggerRampMs` above 0, a key or mouse button bound to `L2` or `R2` presses the analog trigger gradually and lets it go the same way, for throttle and brake in racing titles. A pressed controller trigger is not slowed, and the larger of the two values is passed on.

## Shake

While the `Shake` input is held (X by default), the controller reports its acceleration and angular velocity swinging back and forth every 50 ms, for titles that ask the player to shake the controller. A controller's own motion sensors are used otherwise.

## Mouse as the right stick

`ToggleMouse` (the middle mouse button or F7 by default; a mouse without a middle button can use F7) captures the mouse and drives the right stick from how fast the mouse moves. Capture uses raw relative input, so the pointer speed and acceleration set in the operating system don't apply. Press it again to release the mouse. Capture also ends, and every held key is let go, when the window loses focus.

The stick tilts in the direction the mouse moves, and further the faster it moves. Speed is measured in counts per second over the time between updates, so the same hand speed gives the same tilt at any frame rate. Any movement tilts the stick at least `MouseDeadzoneOffset`, so slow movement isn't lost inside the game's own deadzone. When the mouse stops, the stick eases back and is centered `MouseDecayMs` after the last movement.

For example, for a game with a small deadzone that needs fast turns:

```ini
MouseSensitivity = 1.6
MouseDeadzoneOffset = 0.12
MouseCurve = 1.3
```

Earlier versions sampled the mouse every 33 ms and tilted the stick at least halfway for any movement. `MouseDeadzoneOffset = 0.5`, `MouseSmoothingMs = 0` and `MouseDecayMs = 33` come close to that behavior.

An invalid line reports the file and line number and stops input initialization. If the configured file does not exist or cannot be read, AnyPS5 reports an error. With no `anyps5-input.ini` and no `ANYPS5_INPUT_CONFIG`, the built-in mapping is used.
