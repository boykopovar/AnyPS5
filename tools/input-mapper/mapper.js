const InputMapper = (() => {
    const scancodes = [
        [4, 'A', 'A'],
        [5, 'B', 'B'],
        [6, 'C', 'C'],
        [7, 'D', 'D'],
        [8, 'E', 'E'],
        [9, 'F', 'F'],
        [10, 'G', 'G'],
        [11, 'H', 'H'],
        [12, 'I', 'I'],
        [13, 'J', 'J'],
        [14, 'K', 'K'],
        [15, 'L', 'L'],
        [16, 'M', 'M'],
        [17, 'N', 'N'],
        [18, 'O', 'O'],
        [19, 'P', 'P'],
        [20, 'Q', 'Q'],
        [21, 'R', 'R'],
        [22, 'S', 'S'],
        [23, 'T', 'T'],
        [24, 'U', 'U'],
        [25, 'V', 'V'],
        [26, 'W', 'W'],
        [27, 'X', 'X'],
        [28, 'Y', 'Y'],
        [29, 'Z', 'Z'],
        [30, '1', '1'],
        [31, '2', '2'],
        [32, '3', '3'],
        [33, '4', '4'],
        [34, '5', '5'],
        [35, '6', '6'],
        [36, '7', '7'],
        [37, '8', '8'],
        [38, '9', '9'],
        [39, '0', '0'],
        [40, 'RETURN', 'Return'],
        [41, 'ESCAPE', 'Escape'],
        [42, 'BACKSPACE', 'Backspace'],
        [43, 'TAB', 'Tab'],
        [44, 'SPACE', 'Space'],
        [45, 'MINUS', '-'],
        [46, 'EQUALS', '='],
        [47, 'LEFTBRACKET', '['],
        [48, 'RIGHTBRACKET', ']'],
        [49, 'BACKSLASH', '\\'],
        [50, 'NONUSHASH', '#'],
        [51, 'SEMICOLON', ';'],
        [52, 'APOSTROPHE', '\''],
        [53, 'GRAVE', '`'],
        [54, 'COMMA', ','],
        [55, 'PERIOD', '.'],
        [56, 'SLASH', '/'],
        [57, 'CAPSLOCK', 'CapsLock'],
        [58, 'F1', 'F1'],
        [59, 'F2', 'F2'],
        [60, 'F3', 'F3'],
        [61, 'F4', 'F4'],
        [62, 'F5', 'F5'],
        [63, 'F6', 'F6'],
        [64, 'F7', 'F7'],
        [65, 'F8', 'F8'],
        [66, 'F9', 'F9'],
        [67, 'F10', 'F10'],
        [68, 'F11', 'F11'],
        [69, 'F12', 'F12'],
        [70, 'PRINTSCREEN', 'PrintScreen'],
        [71, 'SCROLLLOCK', 'ScrollLock'],
        [72, 'PAUSE', 'Pause'],
        [73, 'INSERT', 'Insert'],
        [74, 'HOME', 'Home'],
        [75, 'PAGEUP', 'PageUp'],
        [76, 'DELETE', 'Delete'],
        [77, 'END', 'End'],
        [78, 'PAGEDOWN', 'PageDown'],
        [79, 'RIGHT', 'Right'],
        [80, 'LEFT', 'Left'],
        [81, 'DOWN', 'Down'],
        [82, 'UP', 'Up'],
        [83, 'NUMLOCKCLEAR', 'Numlock'],
        [84, 'KP_DIVIDE', 'Keypad /'],
        [85, 'KP_MULTIPLY', 'Keypad *'],
        [86, 'KP_MINUS', 'Keypad -'],
        [87, 'KP_PLUS', 'Keypad +'],
        [88, 'KP_ENTER', 'Keypad Enter'],
        [89, 'KP_1', 'Keypad 1'],
        [90, 'KP_2', 'Keypad 2'],
        [91, 'KP_3', 'Keypad 3'],
        [92, 'KP_4', 'Keypad 4'],
        [93, 'KP_5', 'Keypad 5'],
        [94, 'KP_6', 'Keypad 6'],
        [95, 'KP_7', 'Keypad 7'],
        [96, 'KP_8', 'Keypad 8'],
        [97, 'KP_9', 'Keypad 9'],
        [98, 'KP_0', 'Keypad 0'],
        [99, 'KP_PERIOD', 'Keypad .'],
        [101, 'APPLICATION', 'Application'],
        [102, 'POWER', 'Power'],
        [103, 'KP_EQUALS', 'Keypad ='],
        [104, 'F13', 'F13'],
        [105, 'F14', 'F14'],
        [106, 'F15', 'F15'],
        [107, 'F16', 'F16'],
        [108, 'F17', 'F17'],
        [109, 'F18', 'F18'],
        [110, 'F19', 'F19'],
        [111, 'F20', 'F20'],
        [112, 'F21', 'F21'],
        [113, 'F22', 'F22'],
        [114, 'F23', 'F23'],
        [115, 'F24', 'F24'],
        [116, 'EXECUTE', 'Execute'],
        [117, 'HELP', 'Help'],
        [118, 'MENU', 'Menu'],
        [119, 'SELECT', 'Select'],
        [120, 'STOP', 'Stop'],
        [121, 'AGAIN', 'Again'],
        [122, 'UNDO', 'Undo'],
        [123, 'CUT', 'Cut'],
        [124, 'COPY', 'Copy'],
        [125, 'PASTE', 'Paste'],
        [126, 'FIND', 'Find'],
        [127, 'MUTE', 'Mute'],
        [128, 'VOLUMEUP', 'VolumeUp'],
        [129, 'VOLUMEDOWN', 'VolumeDown'],
        [133, 'KP_COMMA', 'Keypad ,'],
        [134, 'KP_EQUALSAS400', 'Keypad = (AS400)'],
        [153, 'ALTERASE', 'AltErase'],
        [154, 'SYSREQ', 'SysReq'],
        [155, 'CANCEL', 'Cancel'],
        [156, 'CLEAR', 'Clear'],
        [157, 'PRIOR', 'Prior'],
        [158, 'RETURN2', 'Return'],
        [159, 'SEPARATOR', 'Separator'],
        [160, 'OUT', 'Out'],
        [161, 'OPER', 'Oper'],
        [162, 'CLEARAGAIN', 'Clear / Again'],
        [163, 'CRSEL', 'CrSel'],
        [164, 'EXSEL', 'ExSel'],
        [176, 'KP_00', 'Keypad 00'],
        [177, 'KP_000', 'Keypad 000'],
        [178, 'THOUSANDSSEPARATOR', 'ThousandsSeparator'],
        [179, 'DECIMALSEPARATOR', 'DecimalSeparator'],
        [180, 'CURRENCYUNIT', 'CurrencyUnit'],
        [181, 'CURRENCYSUBUNIT', 'CurrencySubUnit'],
        [182, 'KP_LEFTPAREN', 'Keypad ('],
        [183, 'KP_RIGHTPAREN', 'Keypad )'],
        [184, 'KP_LEFTBRACE', 'Keypad {'],
        [185, 'KP_RIGHTBRACE', 'Keypad }'],
        [186, 'KP_TAB', 'Keypad Tab'],
        [187, 'KP_BACKSPACE', 'Keypad Backspace'],
        [188, 'KP_A', 'Keypad A'],
        [189, 'KP_B', 'Keypad B'],
        [190, 'KP_C', 'Keypad C'],
        [191, 'KP_D', 'Keypad D'],
        [192, 'KP_E', 'Keypad E'],
        [193, 'KP_F', 'Keypad F'],
        [194, 'KP_XOR', 'Keypad XOR'],
        [195, 'KP_POWER', 'Keypad ^'],
        [196, 'KP_PERCENT', 'Keypad %'],
        [197, 'KP_LESS', 'Keypad <'],
        [198, 'KP_GREATER', 'Keypad >'],
        [199, 'KP_AMPERSAND', 'Keypad &'],
        [200, 'KP_DBLAMPERSAND', 'Keypad &&'],
        [201, 'KP_VERTICALBAR', 'Keypad |'],
        [202, 'KP_DBLVERTICALBAR', 'Keypad ||'],
        [203, 'KP_COLON', 'Keypad :'],
        [204, 'KP_HASH', 'Keypad #'],
        [205, 'KP_SPACE', 'Keypad Space'],
        [206, 'KP_AT', 'Keypad @'],
        [207, 'KP_EXCLAM', 'Keypad !'],
        [208, 'KP_MEMSTORE', 'Keypad MemStore'],
        [209, 'KP_MEMRECALL', 'Keypad MemRecall'],
        [210, 'KP_MEMCLEAR', 'Keypad MemClear'],
        [211, 'KP_MEMADD', 'Keypad MemAdd'],
        [212, 'KP_MEMSUBTRACT', 'Keypad MemSubtract'],
        [213, 'KP_MEMMULTIPLY', 'Keypad MemMultiply'],
        [214, 'KP_MEMDIVIDE', 'Keypad MemDivide'],
        [215, 'KP_PLUSMINUS', 'Keypad +/-'],
        [216, 'KP_CLEAR', 'Keypad Clear'],
        [217, 'KP_CLEARENTRY', 'Keypad ClearEntry'],
        [218, 'KP_BINARY', 'Keypad Binary'],
        [219, 'KP_OCTAL', 'Keypad Octal'],
        [220, 'KP_DECIMAL', 'Keypad Decimal'],
        [221, 'KP_HEXADECIMAL', 'Keypad Hexadecimal'],
        [224, 'LCTRL', 'Left Ctrl'],
        [225, 'LSHIFT', 'Left Shift'],
        [226, 'LALT', 'Left Alt'],
        [227, 'LGUI', 'Left GUI'],
        [228, 'RCTRL', 'Right Ctrl'],
        [229, 'RSHIFT', 'Right Shift'],
        [230, 'RALT', 'Right Alt'],
        [231, 'RGUI', 'Right GUI'],
        [257, 'MODE', 'ModeSwitch'],
        [258, 'AUDIONEXT', 'AudioNext'],
        [259, 'AUDIOPREV', 'AudioPrev'],
        [260, 'AUDIOSTOP', 'AudioStop'],
        [261, 'AUDIOPLAY', 'AudioPlay'],
        [262, 'AUDIOMUTE', 'AudioMute'],
        [263, 'MEDIASELECT', 'MediaSelect'],
        [264, 'WWW', 'WWW'],
        [265, 'MAIL', 'Mail'],
        [266, 'CALCULATOR', 'Calculator'],
        [267, 'COMPUTER', 'Computer'],
        [268, 'AC_SEARCH', 'AC Search'],
        [269, 'AC_HOME', 'AC Home'],
        [270, 'AC_BACK', 'AC Back'],
        [271, 'AC_FORWARD', 'AC Forward'],
        [272, 'AC_STOP', 'AC Stop'],
        [273, 'AC_REFRESH', 'AC Refresh'],
        [274, 'AC_BOOKMARKS', 'AC Bookmarks'],
        [275, 'BRIGHTNESSDOWN', 'BrightnessDown'],
        [276, 'BRIGHTNESSUP', 'BrightnessUp'],
        [277, 'DISPLAYSWITCH', 'DisplaySwitch'],
        [278, 'KBDILLUMTOGGLE', 'KBDIllumToggle'],
        [279, 'KBDILLUMDOWN', 'KBDIllumDown'],
        [280, 'KBDILLUMUP', 'KBDIllumUp'],
        [281, 'EJECT', 'Eject'],
        [282, 'SLEEP', 'Sleep'],
        [283, 'APP1', 'App1'],
        [284, 'APP2', 'App2'],
        [285, 'AUDIOREWIND', 'AudioRewind'],
        [286, 'AUDIOFASTFORWARD', 'AudioFastForward'],
        [287, 'SOFTLEFT', 'SoftLeft'],
        [288, 'SOFTRIGHT', 'SoftRight'],
        [289, 'CALL', 'Call'],
        [290, 'ENDCALL', 'EndCall']
    ];

    const capturedKeys = {
        Enter: 'RETURN',
        Escape: 'ESCAPE',
        Backspace: 'BACKSPACE',
        Tab: 'TAB',
        Space: 'SPACE',
        Minus: 'MINUS',
        Equal: 'EQUALS',
        BracketLeft: 'LEFTBRACKET',
        BracketRight: 'RIGHTBRACKET',
        Backslash: 'BACKSLASH',
        Semicolon: 'SEMICOLON',
        Quote: 'APOSTROPHE',
        Backquote: 'GRAVE',
        Comma: 'COMMA',
        Period: 'PERIOD',
        Slash: 'SLASH',
        CapsLock: 'CAPSLOCK',
        PrintScreen: 'PRINTSCREEN',
        ScrollLock: 'SCROLLLOCK',
        Pause: 'PAUSE',
        Insert: 'INSERT',
        Home: 'HOME',
        PageUp: 'PAGEUP',
        Delete: 'DELETE',
        End: 'END',
        PageDown: 'PAGEDOWN',
        ArrowRight: 'RIGHT',
        ArrowLeft: 'LEFT',
        ArrowDown: 'DOWN',
        ArrowUp: 'UP',
        NumLock: 'NUMLOCKCLEAR',
        NumpadDivide: 'KP_DIVIDE',
        NumpadMultiply: 'KP_MULTIPLY',
        NumpadSubtract: 'KP_MINUS',
        NumpadAdd: 'KP_PLUS',
        NumpadEnter: 'KP_ENTER',
        NumpadDecimal: 'KP_PERIOD',
        IntlBackslash: 'NONUSBACKSLASH',
        ContextMenu: 'APPLICATION',
        IntlRo: 'INTERNATIONAL1',
        KanaMode: 'INTERNATIONAL2',
        IntlYen: 'INTERNATIONAL3',
        Convert: 'INTERNATIONAL4',
        NonConvert: 'INTERNATIONAL5',
        ControlLeft: 'LCTRL',
        ShiftLeft: 'LSHIFT',
        AltLeft: 'LALT',
        MetaLeft: 'LGUI',
        ControlRight: 'RCTRL',
        ShiftRight: 'RSHIFT',
        AltRight: 'RALT',
        MetaRight: 'RGUI'
    };
    for (const letter of 'ABCDEFGHIJKLMNOPQRSTUVWXYZ') capturedKeys['Key' + letter] = letter;
    for (const digit of '0123456789') {
        capturedKeys['Digit' + digit] = digit;
        capturedKeys['Numpad' + digit] = 'KP_' + digit;
    }
    for (let number = 1; number <= 24; ++number) capturedKeys['F' + number] = 'F' + number;

    const actions = [
        { name: 'Cross', button: true },
        { name: 'Circle', button: true },
        { name: 'Triangle', button: true },
        { name: 'Square', button: true },
        { name: 'L1', button: true },
        { name: 'R1', button: true },
        { name: 'L2', button: true },
        { name: 'R2', button: true },
        { name: 'L3', button: true },
        { name: 'R3', button: true },
        { name: 'Options', button: true },
        { name: 'Up', button: true },
        { name: 'Right', button: true },
        { name: 'Down', button: true },
        { name: 'Left', button: true },
        { name: 'LeftStickLeft', button: false },
        { name: 'LeftStickRight', button: false },
        { name: 'LeftStickUp', button: false },
        { name: 'LeftStickDown', button: false },
        { name: 'RightStickLeft', button: false },
        { name: 'RightStickRight', button: false },
        { name: 'RightStickUp', button: false },
        { name: 'RightStickDown', button: false },
        { name: 'TouchLeft', button: false },
        { name: 'TouchRight', button: false },
        { name: 'ToggleMouse', button: false },
        { name: 'ToggleFullscreen', button: false }
    ];

    const builtIn = [
        ['ToggleFullscreen', 'KEY:F11'],
        ['Cross', 'KEY:Return'],
        ['Cross', 'KEY:Space'],
        ['Options', 'KEY:Escape'],
        ['Triangle', 'KEY:I'],
        ['Circle', 'KEY:C'],
        ['L1', 'KEY:Q'],
        ['R1', 'KEY:E'],
        ['R1', 'KEY:Left Alt'],
        ['R1', 'KEY:Right Alt'],
        ['L3', 'KEY:Left Shift'],
        ['L3', 'KEY:Right Shift'],
        ['R3', 'KEY:Left Ctrl'],
        ['R3', 'KEY:Right Ctrl'],
        ['Up', 'KEY:Up'],
        ['Right', 'KEY:Right'],
        ['Down', 'KEY:Down'],
        ['Left', 'KEY:Left'],
        ['LeftStickLeft', 'KEY:A'],
        ['LeftStickRight', 'KEY:D'],
        ['LeftStickUp', 'KEY:W'],
        ['LeftStickDown', 'KEY:S'],
        ['RightStickLeft', 'KEY:F'],
        ['RightStickRight', 'KEY:H'],
        ['RightStickUp', 'KEY:T'],
        ['RightStickDown', 'KEY:G'],
        ['TouchLeft', 'KEY:Backspace'],
        ['TouchRight', 'KEY:Tab'],
        ['ToggleMouse', 'MOUSE:Middle'],
        ['Square', 'MOUSE:Left'],
        ['R2', 'MOUSE:Right'],
        ['Up', 'WHEEL:Up'],
        ['Down', 'WHEEL:Down']
    ];

    const mouseButtons = ['Left', 'Middle', 'Right', 'X1', 'X2'];

    function upper(text) {
        return text.replace(/[a-z]/g, (character) => character.toUpperCase());
    }

    function trim(text) {
        return text.replace(/^[ \t\n\v\f\r]+|[ \t\n\v\f\r]+$/g, '');
    }

    const keysByName = new Map();
    for (const [key, , name] of scancodes) {
        if (!keysByName.has(upper(name))) keysByName.set(upper(name), key);
    }
    const keysById = new Map(scancodes.map(([key, id]) => [id, key]));
    const namesByKey = new Map(scancodes.map(([key, , name]) => [key, name]));

    function keyFromName(name) {
        return keysByName.get(upper(name)) || 0;
    }

    function keyName(key) {
        return namesByKey.get(key) || '';
    }

    function keyFromCode(code) {
        if (!Object.hasOwn(capturedKeys, code)) return null;
        const id = capturedKeys[code];
        return { id, key: keysById.get(id) || 0 };
    }

    function findAction(name) {
        const wanted = upper(name);
        return actions.find((action) => upper(action.name) === wanted) || null;
    }

    function parseSource(action, source) {
        const separator = source.indexOf(':');
        if (separator < 0) throw new Error('source must use TYPE:VALUE format');
        const type = upper(trim(source.slice(0, separator)));
        const value = trim(source.slice(separator + 1));
        if (!value) throw new Error('source value is empty');
        if (type === 'KEY') {
            const key = keyFromName(value);
            if (!key) throw new Error(`unknown SDL key name '${value}'`);
            return { type: 'KEY', key };
        }
        if (type === 'MOUSE') {
            if (action.name === 'ToggleFullscreen') throw new Error('ToggleFullscreen can only use a keyboard key');
            const button = mouseButtons.find((candidate) => upper(candidate) === upper(value));
            if (!button) throw new Error(`unknown mouse button '${value}'`);
            return { type: 'MOUSE', button };
        }
        if (type === 'WHEEL') {
            if (!action.button) throw new Error('mouse wheel can only map to a pad button');
            const direction = upper(value);
            if (direction === 'UP') return { type: 'WHEEL', direction: 'Up' };
            if (direction === 'DOWN') return { type: 'WHEEL', direction: 'Down' };
            throw new Error('mouse wheel direction must be UP or DOWN');
        }
        throw new Error('source type must be KEY, MOUSE or WHEEL');
    }

    function format(binding) {
        if (binding.type === 'KEY') return 'KEY:' + keyName(binding.key);
        if (binding.type === 'MOUSE') return 'MOUSE:' + binding.button;
        return 'WHEEL:' + binding.direction;
    }

    function same(left, right) {
        return left.type === right.type && left.key === right.key && left.button === right.button &&
            left.direction === right.direction;
    }

    function sameSet(left, right) {
        return left.length === right.length && left.every((binding) => right.some((other) => same(binding, other)));
    }

    function defaults() {
        return new Map([...builtInBindings].map(([name, list]) => [name, list.map((binding) => ({ ...binding }))]));
    }

    function parse(text) {
        const bindings = defaults();
        const overridden = new Set();
        const errors = [];
        for (const [index, raw] of text.split('\n').entries()) {
            const line = index === 0 && raw.startsWith('﻿') ? raw.slice(1) : raw;
            const comment = line.search(/[#;]/);
            const content = trim(comment < 0 ? line : line.slice(0, comment));
            if (!content) continue;
            const separator = content.indexOf('=');
            if (separator < 0) {
                errors.push({ line: index + 1, reason: 'expected Action = TYPE:VALUE' });
                continue;
            }
            const actionName = trim(content.slice(0, separator));
            const source = trim(content.slice(separator + 1));
            const action = findAction(actionName);
            if (!action) {
                errors.push({ line: index + 1, reason: `unknown action '${actionName}'` });
                continue;
            }
            if (!source) {
                errors.push({ line: index + 1, reason: 'source is empty' });
                continue;
            }
            let binding;
            try {
                binding = parseSource(action, source);
            } catch (error) {
                errors.push({ line: index + 1, reason: error.message });
                continue;
            }
            if (!overridden.has(action.name)) {
                overridden.add(action.name);
                bindings.set(action.name, []);
            }
            const current = bindings.get(action.name);
            if (!current.some((existing) => same(existing, binding))) current.push(binding);
        }
        return { bindings, errors };
    }

    function changed(bindings, name) {
        return !sameSet(bindings.get(name), builtInBindings.get(name));
    }

    function serialize(bindings) {
        const lines = [];
        for (const action of actions) {
            if (!changed(bindings, action.name)) continue;
            const current = bindings.get(action.name);
            if (current.length === 0) throw new Error(`${action.name} has no binding and cannot be written`);
            for (const binding of current) lines.push(`${action.name} = ${format(binding)}`);
        }
        return lines.length === 0 ? '' : lines.join('\n') + '\n';
    }

    function problem(name, binding) {
        const action = findAction(name);
        if (binding.type === 'KEY' && !binding.key) return 'SDL 2 has no name for this key, so anyps5-input.ini cannot refer to it';
        const source = format(binding);
        try {
            parseSource(action, source);
        } catch (error) {
            return error.message;
        }
        if (/[#;]/.test(source)) {
            return `'${source.slice(source.indexOf(':') + 1)}' cannot be written: AnyPS5 reads # and ; as the start of a comment`;
        }
        const result = parse(`${action.name} = ${source}\n`);
        const written = result.bindings.get(action.name);
        if (result.errors.length !== 0 || written.length !== 1 || !same(written[0], binding)) {
            return `${source} does not read back as the same input`;
        }
        return '';
    }

    function conflicts(bindings) {
        const users = [];
        for (const action of actions) {
            for (const binding of bindings.get(action.name)) {
                const entry = users.find((candidate) => same(candidate.binding, binding));
                if (entry) entry.actions.push(action.name);
                else users.push({ binding, actions: [action.name] });
            }
        }
        return users.filter((entry) => entry.actions.length > 1);
    }

    const builtInBindings = new Map(actions.map((action) => [action.name, []]));
    for (const [name, source] of builtIn) builtInBindings.get(name).push(parseSource(findAction(name), source));

    return {
        actions,
        builtIn,
        capturedKeys,
        mouseButtons,
        scancodes,
        changed,
        conflicts,
        defaults,
        format,
        keyFromCode,
        keyFromName,
        keyName,
        parse,
        problem,
        same,
        serialize
    };
})();
