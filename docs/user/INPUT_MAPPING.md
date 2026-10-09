# Controller, keyboard and mouse input

AnyPS5 accepts SDL-mapped game controllers as well as its built-in keyboard and mouse bindings. Xbox-style controllers and PlayStation controllers recognized by SDL use the standard PS button layout; sticks and analog triggers are passed through, and controllers can be connected or disconnected while the game is running. The first recognized controller is used. AnyPS5 uses its built-in keyboard and mouse bindings when no configuration file is present. To change selected keyboard and mouse bindings, create `anyps5-input.ini` beside the generated game executable. Set `ANYPS5_INPUT_CONFIG` to use a file at another path.

For an Xbox-layout controller, A/B/X/Y map to Cross/Circle/Square/Triangle, LB/RB map to L1/R1, and LT/RT map to analog L2/R2. Start maps to Options. D-pad and stick-click buttons are supported. On a PlayStation controller, the touchpad click button is forwarded; finger touch coordinates are not implemented.

Each non-empty line has the form `Action = Type:Value`. Action names are case-insensitive. A `#` or `;` starts a comment. The first line for an action replaces its built-in bindings; later lines for the same action add alternate inputs. Duplicate entries for the same action are ignored. Actions omitted from the file keep their built-in bindings. UTF-8 files with or without a byte-order mark are supported.

Supported input sources are:

- `KEY:Return`, `KEY:Space`, or another key name accepted by SDL.
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

For static hosting, serve `index.html`, `styles.css`, `app.js`, `mapping.js`, `keys.js`, `capture.js`, `validation.js`, `i18n.js` and `dropdown.js` from one directory. For the existing GitHub Pages deployment, these files can be copied into `out/input-mapper/` after the Progress workflow generates `out`. The progress dashboard remains at `out/index.html`; publication and a dashboard link are left for maintainer review.
