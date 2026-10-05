# Controller, keyboard and mouse input

AnyPS5 accepts SDL-mapped game controllers as well as its built-in keyboard and mouse bindings. Xbox-style controllers and PlayStation controllers recognized by SDL use the standard PS button layout; sticks and analog triggers are passed through, and controllers can be connected or disconnected while the game is running. The first recognized controller is used. AnyPS5 uses its built-in keyboard and mouse bindings when no configuration file is present. To change selected keyboard and mouse bindings, create `anyps5-input.ini` beside the generated game executable. Set `ANYPS5_INPUT_CONFIG` to use a file at another path.

For an Xbox-layout controller, A/B/X/Y map to Cross/Circle/Square/Triangle, LB/RB map to L1/R1, and LT/RT map to analog L2/R2. Start maps to Options. D-pad and stick-click buttons are supported. On a PlayStation controller, the touchpad click button is forwarded; finger touch coordinates are not implemented.

Each non-empty line has the form `Action = Type:Value`. Action names are case-insensitive. A `#` or `;` starts a comment. The first line for an action replaces its built-in bindings; later lines for the same action add alternate inputs. Duplicate entries for the same action are ignored. Actions omitted from the file keep their built-in bindings. UTF-8 files with or without a byte-order mark are supported.

Supported input sources are:

- `KEY:Return`, `KEY:Space`, or another key name accepted by SDL.
- `SCANCODE:51` for an SDL scancode whose key name contains a comment marker, such as semicolon.
- `MOUSE:Left`, `MOUSE:Middle`, `MOUSE:Right`, `MOUSE:X1`, or `MOUSE:X2`.
- `WHEEL:Up` or `WHEEL:Down` for pad buttons.

Game-controller bindings are enabled automatically and are not affected by keyboard or mouse overrides in this file.

Supported actions are `Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left`, `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown`, `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown`, `TouchLeft`, `TouchRight`, `ToggleMouse`, and `ToggleFullscreen`.

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

An invalid line reports the file and line number and stops input initialization. If the configured file does not exist or cannot be read, AnyPS5 reports an error. With no `anyps5-input.ini` and no `ANYPS5_INPUT_CONFIG`, the built-in mapping is used.

## Native configuration editor

Run `anyps5-input-config` (`anyps5-input-config.exe` on Windows) without a game, or press F10 in the game window. Both open the same native editor in a separate window. F10 is reserved for opening the editor. The game continues running while the editor is open, but pad, keyboard and mouse input is withheld. Closing the editor does not close the game.

The editor uses `ANYPS5_INPUT_CONFIG`, or `anyps5-input.ini` beside its executable. To configure a game from the standalone tool, pass its input file explicitly:

```sh
anyps5-input-config --config /path/to/game/anyps5-input.ini
```

The Windows equivalent is `anyps5-input-config.exe --config "C:\Games\Title\anyps5-input.ini"`. The destination directory must exist. Controller profiles and SDL mappings use files in the same directory as the input file.

**Keyboard / Mouse** lists the supported PS5 actions and their alternate bindings. Remove an entry or capture another key, mouse button or wheel direction. Default restores the action's built-in bindings. Saving writes the complete effective configuration. `Action = NONE` clears an action's keyboard and mouse bindings; a later binding for that action adds an input again. Existing files retain their previous interpretation.

**PS5 Commands** selects a controller and edits its logical SDL button and axis sources. Axis inversion, disabled controls and an analog preview are available. Profiles are stored in `anyps5-controller.ini` under a `[GUID]` section. Controllers with the same SDL GUID share a profile. Targets omitted from a profile retain their standard bindings. For example:

```ini
[030000005e0400008e02000000000000]
Cross = BUTTON:b
Circle = BUTTON:a
LeftStickY = AXIS:lefty:inverted
TouchPad = NONE
```

Button targets are Cross, Circle, Square, Triangle, L1, R1, Options, L3, R3, Up, Right, Down, Left and TouchPad. Axis targets are LeftStickX, LeftStickY, RightStickX, RightStickY, L2 and R2. Sources use SDL controller names. `NONE` disables a target.

**SDL Mapping** creates physical mappings, including for joysticks not recognized as game controllers. Release the controls before capture. Move a stick right or down, or press a trigger fully when prompted. The assistant samples resting axes, detects buttons, axes and cardinal hat directions, and requires confirmation for each input. Skip controls absent from the device. Save writes `anyps5-gamecontrollerdb.txt` using SDL mapping records with the current platform. The database is loaded before controller detection. Unfinished mappings are retained while selecting or reconnecting devices within the same editor session.

Save applies only to the current tab. During gameplay, saved changes apply immediately; SDL mapping changes reopen the active controller. Closing with unsaved edits requires confirmation. Invalid configuration or a failed write reports an error; failed writes leave the destination file intact. Saving normalizes the file and does not preserve comments. Finger touch data, sensors and controller output remain handled by the existing SDL runtime.
