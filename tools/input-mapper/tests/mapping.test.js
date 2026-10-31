import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {actions, defaults, mouseValues, padButtons, typesForAction} from '../mapping.js';
import {exportableKeys, keyNames} from '../keys.js';
import {captureKey} from '../capture.js';
import {parseIni, serialize, settings, validate, validateSettings} from '../validation.js';

const binding = (action, type = 'KEY', value = 'F') => ({action, type, value});

test('action names and pad button categories agree with the current native parser', () => {
    const source = readFileSync(new URL('../../../core/libs/prx/libSceVideoOut/src/InputMapping.cpp', import.meta.url), 'utf8');
    const definitions = [...source.matchAll(/InputAction\{"(\w+)", Pad::InputControl::(\w+),/gu)];
    assert.deepEqual([...actions].sort(), definitions.map(match => match[1]).sort());
    assert.deepEqual([...padButtons].sort(), definitions.filter(match => match[2] === 'Button').map(match => match[1]).sort());
    assert.match(source, /SDL_GetScancodeFromName\(keyName\.c_str\(\)\)/u);
    const keyboardOnly = [...source.matchAll(/if \(action\.control == Pad::InputControl::(\w+)\) \{\s*throw std::runtime_error\("\w+ can only use a keyboard key"\);/gu)].map(match => match[1]);
    assert.deepEqual(actions.filter(action => typesForAction(action).length === 1).sort(), keyboardOnly.sort());
    assert.match(source, /if \(action\.control != Pad::InputControl::Button\)/u);
});

test('built-in conflict catalogue agrees with the current native bindings', () => {
    const source = readFileSync(new URL('../../../core/libs/prx/libScePad/include/InputMapping.hpp', import.meta.url), 'utf8');
    const special = {RETURN: 'Return', SPACE: 'Space', ESCAPE: 'Escape', BACKSPACE: 'Backspace', TAB: 'Tab', UP: 'Up', RIGHT: 'Right', DOWN: 'Down', LEFT: 'Left', LALT: 'Left Alt', RALT: 'Right Alt', LSHIFT: 'Left Shift', RSHIFT: 'Right Shift', LCTRL: 'Left Ctrl', RCTRL: 'Right Ctrl'};
    const native = Object.fromEntries(actions.map(action => [action, []]));
    for (const match of source.matchAll(/InputBinding\{SDL_SCANCODE_(\w+), MouseButton::(\w+), InputControl::(\w+)(?:, PadButton::(\w+))?(?:, (-?\d+))?\}/gu)) {
        const [, key, mouse, control, button, wheel] = match;
        const action = control === 'Button' ? button : control;
        const value = key !== 'UNKNOWN' ? `KEY:${special[key] || key}` : mouse !== 'None' ? `MOUSE:${mouse}` : `WHEEL:${wheel === '1' ? 'Up' : 'Down'}`;
        native[action].push(value);
    }
    assert.deepEqual(native, defaults);
});

test('setting names and order agree with the current native parser', () => {
    const source = readFileSync(new URL('../../../core/libs/prx/libSceVideoOut/src/InputMapping.cpp', import.meta.url), 'utf8');
    const definitions = [...source.matchAll(/InputSettingName\{"(\w+)", InputSetting::\w+\}/gu)].map(match => match[1]);
    assert.deepEqual(settings.map(setting => setting.name), definitions);
    assert.equal(new Set(definitions).size, definitions.length);
});

test('settings from a documented configuration import and round trip', () => {
    const source = '\uFEFF# tuned\r\nCross = KEY:Return\r\nLeftStickDeadzone = 12\r\nMouseSensitivity = 2.50\r\nStickModifierScale = 0.4\r\nTriggerRampMs = 250\r\n';
    const imported = parseIni(source);
    assert.deepEqual(imported.errors, []);
    assert.deepEqual(imported.entries, [binding('Cross', 'KEY', 'Return')]);
    assert.deepEqual(imported.settings, [
        {name: 'LeftStickDeadzone', value: '12'},
        {name: 'MouseSensitivity', value: '2.50'},
        {name: 'StickModifierScale', value: '0.4'},
        {name: 'TriggerRampMs', value: '250'}
    ]);
    assert.equal(serialize(imported.entries, imported.settings),
        'Cross = KEY:Return\nLeftStickDeadzone = 12\nMouseSensitivity = 2.50\nStickModifierScale = 0.4\nTriggerRampMs = 250\n');
    assert.equal(serialize(imported.entries), 'Cross = KEY:Return\n');
});

test('settings-only files import without bindings', () => {
    const imported = parseIni('MouseCurve = 1.3\nMouseDecayMs = 33\n');
    assert.deepEqual(imported.errors, []);
    assert.deepEqual(imported.entries, []);
    assert.equal(serialize(imported.entries, imported.settings), 'MouseCurve = 1.3\nMouseDecayMs = 33\n');
});

test('setting names canonicalize like the native parser', () => {
    const imported = parseIni('leftstickdeadzone = 8\nMOUSESENSITIVITY = 1e0\n');
    assert.deepEqual(imported.errors, []);
    assert.deepEqual(imported.settings, [{name: 'LeftStickDeadzone', value: '8'}, {name: 'MouseSensitivity', value: '1e0'}]);
});

test('a signed zero deadzone imports like the native parser', () => {
    const imported = parseIni('LeftStickDeadzone = -0\nRightStickDeadzone = -00\n');
    assert.deepEqual(imported.errors, []);
    assert.deepEqual(imported.settings, [{name: 'LeftStickDeadzone', value: '-0'}, {name: 'RightStickDeadzone', value: '-00'}]);
    assert.equal(serialize(imported.entries, imported.settings), 'LeftStickDeadzone = -0\nRightStickDeadzone = -00\n');
});

test('out-of-range and malformed setting values report their line', () => {
    const cases = {
        'LeftStickDeadzone = 91': 'LeftStickDeadzone must be a whole percentage from 0 to 90.',
        'LeftStickDeadzone = 12.5': 'LeftStickDeadzone must be a whole percentage from 0 to 90.',
        'LeftStickDeadzone = +12': 'LeftStickDeadzone must be a whole percentage from 0 to 90.',
        'LeftStickDeadzone = -1': 'LeftStickDeadzone must be a whole percentage from 0 to 90.',
        'MouseSensitivity = 0': 'MouseSensitivity must be a number greater than 0 and at most 10.',
        'MouseSensitivity = 10.1': 'MouseSensitivity must be a number greater than 0 and at most 10.',
        'MouseSensitivity = abc': 'MouseSensitivity must be a number greater than 0 and at most 10.',
        'MouseSensitivity = 1e999': 'MouseSensitivity must be a number greater than 0 and at most 10.',
        'MouseSensitivity = 0x1': 'MouseSensitivity must be a number greater than 0 and at most 10.',
        'MouseSensitivity = +2.5': 'MouseSensitivity must be a number greater than 0 and at most 10.',
        'MouseCurve = 0.1': 'MouseCurve must be a number from 0.2 to 5.',
        'MouseDecayMs = 0': 'MouseDecayMs must be a number from 1 to 1000.',
        'StickModifierScale = 1.1': 'StickModifierScale must be a number from 0.05 to 1.'
    };
    for (const [line, message] of Object.entries(cases)) {
        const result = parseIni(line);
        assert.deepEqual(result.errors, [{line: 1, message}], line);
        assert.deepEqual(result.settings, [], line);
    }
    for (const line of ['MouseSensitivity = 10', 'MouseCurve = 1e0', 'MouseDeadzoneOffset = 0', 'KeyboardRampMs = 2000', 'LeftStickDeadzone = 0', 'RightStickDeadzone = 90']) {
        assert.deepEqual(parseIni(line).errors, [], line);
    }
});

test('a setting set more than once is refused like the native parser', () => {
    const result = parseIni('Cross = KEY:F\nMouseSensitivity = 1\nMouseSensitivity = 2\n');
    assert.deepEqual(result.errors, [{line: 3, message: 'MouseSensitivity is set more than once.'}]);
    assert.deepEqual(result.settings, [{name: 'MouseSensitivity', value: '1'}]);
});

test('serialize refuses invalid settings it is handed', () => {
    for (const configuration of [[{name: 'MouseSensitivity', value: '0'}], [{name: 'MadeUp', value: '1'}], [{name: 'MouseCurve', value: '9'}], [{name: 'MouseCurve', value: '1'}, {name: 'MouseCurve', value: '2'}], null]) {
        assert.throws(() => serialize([], configuration));
        assert.ok(validateSettings(configuration).errors.length >= 1);
    }
    assert.deepEqual(validateSettings([{name: 'mousecurve', value: ' 2 '}]), {errors: [], settings: [{name: 'MouseCurve', value: '2'}]});
});

test('every action serializes with a supported key', () => {
    const input = actions.map(action => binding(action));
    assert.equal(serialize(input), actions.map(action => `${action} = KEY:F\n`).join(''));
});

test('multiple inputs are alternatives and omitted actions stay omitted', () => {
    assert.equal(serialize([binding('Cross'), binding('Cross', 'KEY', 'Space')]), 'Cross = KEY:F\nCross = KEY:Space\n');
    assert.equal(serialize([]), '');
    assert.equal(serialize([binding('R2', 'MOUSE', 'Right')]), 'R2 = MOUSE:Right\n');
});

test('case and whitespace normalize; duplicate bindings warn and serialize once', () => {
    const input = [binding(' cross ', ' key ', ' f '), binding('Cross')];
    assert.equal(serialize(input), 'Cross = KEY:F\n');
    assert.equal(validate(input).warnings.filter(warning => warning.message.includes('duplicate')).length, 1);
});

test('invalid and incomplete bindings block export', () => {
    for (const input of [binding('Unknown'), binding('Cross', 'GAMEPAD'), binding('Cross', 'KEY', 'Enter'), binding('Cross', 'KEY', 'KeyF'), binding('Cross', 'KEY', ''), binding('Cross', 'KEY', 'F\nR2 = MOUSE:Left'), null, {}]) {
        assert.ok(validate([input]).errors.length);
        assert.throws(() => serialize([input]));
    }
    assert.ok(validate({}).errors.length);
});

test('all exportable SDL names validate and unsafe comment names are rejected', () => {
    for (const key of exportableKeys) assert.equal(validate([binding('Cross', 'KEY', key)]).errors.length, 0, key);
    for (const key of [';', '#', 'Keypad #']) assert.ok(validate([binding('Cross', 'KEY', key)]).errors.length, key);
    assert.equal(new Set(keyNames).size, keyNames.length);
});

test('mouse values and action restrictions match the current INI format', () => {
    for (const action of actions) {
        for (const value of mouseValues) {
            assert.equal(validate([binding(action, 'MOUSE', value)]).errors.length === 0, !['ToggleFullscreen', 'TogglePerformanceOverlay', 'Screenshot', 'ShowPadStatus', 'VolumeUp', 'VolumeDown', 'ToggleMute'].includes(action));
        }
    }
    assert.ok(validate([binding('Cross', 'MOUSE', 'Button1')]).errors.length);
});

test('wheel directions only map to pad buttons', () => {
    for (const action of actions) {
        for (const value of ['Up', 'Down']) {
            assert.equal(validate([binding(action, 'WHEEL', value)]).errors.length === 0, padButtons.has(action));
        }
        assert.deepEqual(typesForAction(action).includes('WHEEL'), padButtons.has(action));
    }
    assert.ok(validate([binding('Cross', 'WHEEL', 'Left')]).errors.length);
});

test('new utility actions import and round trip without accepting unsupported sources', () => {
    const source = 'StickModifier = MOUSE:X2\nShake = MOUSE:Left\nTogglePerformanceOverlay = KEY:F9\nScreenshot = KEY:F8\nShowPadStatus = KEY:F4\n';
    const imported = parseIni(source);
    assert.deepEqual(imported.errors, []);
    assert.equal(serialize(imported.entries), source);
    for (const action of ['StickModifier', 'Shake', 'TogglePerformanceOverlay', 'Screenshot', 'ShowPadStatus']) {
        assert.ok(validate([binding(action, 'WHEEL', 'Up')]).errors.length);
    }
    for (const action of ['TogglePerformanceOverlay', 'Screenshot', 'ShowPadStatus']) {
        assert.deepEqual(parseIni(`${action} = MOUSE:Left`).errors.map(error => error.line), [1]);
    }
});

test('ShowPadStatus is a keyboard-only action with an F9 default that conflicts like any built-in', () => {
    assert.deepEqual(defaults.ShowPadStatus, ['KEY:F9']);
    assert.deepEqual(typesForAction('ShowPadStatus'), ['KEY']);
    assert.equal(serialize([binding('ShowPadStatus', 'KEY', 'F4')]), 'ShowPadStatus = KEY:F4\n');
    assert.equal(serialize([binding(' showpadstatus ', 'key', 'f9')]), 'ShowPadStatus = KEY:F9\n');
    for (const [type, value] of [['MOUSE', 'Left'], ['MOUSE', 'X2'], ['WHEEL', 'Up'], ['WHEEL', 'Down']]) {
        assert.ok(validate([binding('ShowPadStatus', type, value)]).errors.length, `${type}:${value}`);
    }
    assert.equal(validate([binding('ShowPadStatus', 'KEY', 'F4')]).warnings.length, 0);
    const shared = validate([binding('TogglePerformanceOverlay', 'KEY', 'F9')]);
    assert.ok(shared.warnings.some(warning => warning.message.includes('TogglePerformanceOverlay and ShowPadStatus (built-in)')));
    assert.equal(validate([binding('TogglePerformanceOverlay', 'KEY', 'F9'), binding('ShowPadStatus', 'KEY', 'F4')]).warnings.length, 0);
    assert.ok(Object.values(defaults).flat().filter(source => source === 'KEY:F9').length === 1);
});

test('updated built-in inputs warn until their owning action is overridden', () => {
    for (const [action, type, value] of [['L2', 'KEY', 'R'], ['L2', 'MOUSE', 'X1'], ['ToggleMouse', 'KEY', 'F7'], ['Shake', 'KEY', 'X'], ['Screenshot', 'KEY', 'F12'], ['TogglePerformanceOverlay', 'KEY', 'F10'], ['ShowPadStatus', 'KEY', 'F9']]) {
        const input = binding('Cross', type, value);
        assert.ok(validate([input]).warnings.some(warning => warning.message.includes(`${action} (built-in)`)));
        assert.equal(validate([input, binding(action, 'KEY', 'J')]).warnings.length, 0);
    }
});

test('conflicts warn without blocking, including omitted built-in actions', () => {
    const input = [binding('Cross'), binding('Square')];
    const result = validate(input);
    assert.equal(result.errors.length, 0);
    assert.ok(result.warnings.some(warning => warning.message.includes('Cross and Square and RightStickLeft (built-in)')));
    assert.doesNotThrow(() => serialize(input));
    const overridden = validate([binding('Cross'), binding('RightStickLeft', 'KEY', 'J')]);
    assert.equal(overridden.warnings.length, 0);
});

test('serialization is deterministic in action order and preserves alternative order', () => {
    const input = [binding('ToggleMouse', 'MOUSE', 'X1'), binding('Cross', 'KEY', 'Space'), binding('Cross')];
    assert.equal(serialize(input), 'Cross = KEY:Space\nCross = KEY:F\nToggleMouse = MOUSE:X1\n');
    assert.deepEqual(input, [binding('ToggleMouse', 'MOUSE', 'X1'), binding('Cross', 'KEY', 'Space'), binding('Cross')]);
});

test('import supports BOM, CRLF, inline comments, extra equals and colons in key names', () => {
    const result = parseIni('\uFEFF# config\r\n cross = key : space ; note\r\nCross=KEY:Space\r\nR2=mouse:right\r\nL1=KEY:=\r\nL2=KEY:Keypad :\r\n');
    assert.deepEqual(result.errors, []);
    assert.equal(result.warnings.length, 1);
    assert.deepEqual(result.entries, [binding('Cross', 'KEY', 'Space'), binding('R2', 'MOUSE', 'Right'), binding('L1', 'KEY', '='), binding('L2', 'KEY', 'Keypad :')]);
});

test('import rejects sections, unsupported extensions and malformed lines with line numbers', () => {
    const result = parseIni('[Bindings]\nCross=NONE\nCross=KEY:4\nToggleFullscreen=MOUSE:Left\nLeftStickUp=WHEEL:Up\nCross=KEY:;\nCross=KEY:KeyF\nCross=KEY:\n');
    assert.deepEqual(result.errors.map(error => error.line), [1, 2, 4, 5, 6, 7, 8]);
    assert.deepEqual(result.entries, [binding('Cross', 'KEY', '4')]);
    assert.throws(() => parseIni(null), TypeError);
});

test('round trip is stable for every safe key and supported mouse/wheel source', () => {
    const input = [...exportableKeys.map(value => binding('Cross', 'KEY', value)), ...mouseValues.map(value => binding('Square', 'MOUSE', value)), binding('R2', 'WHEEL', 'Down')];
    const text = serialize(input);
    const imported = parseIni(text);
    assert.equal(imported.errors.length, 0);
    assert.equal(serialize(imported.entries), text);
});

test('capture maps physical positions and distinguishes modifiers and keypad', () => {
    const cases = {KeyA: 'A', Digit1: '1', Enter: 'Return', NumpadEnter: 'Keypad Enter', Numpad1: 'Keypad 1', ControlLeft: 'Left Ctrl', AltRight: 'Right Alt', ArrowUp: 'Up', F24: 'F24', Equal: '=', Backslash: '\\'};
    for (const [code, value] of Object.entries(cases)) assert.deepEqual(captureKey({code, key: 'ignored'}), {value});
    assert.equal(captureKey({code: 'KeyQ', key: 'a'}).value, 'Q');
});

test('captured media and browser keys round trip through INI bindings', () => {
    const keys = {
        MediaTrackNext: 'AudioNext', MediaTrackPrevious: 'AudioPrev', MediaStop: 'AudioStop',
        MediaPlayPause: 'AudioPlay', MediaSelect: 'MediaSelect',
        AudioVolumeMute: 'Mute', AudioVolumeUp: 'VolumeUp', AudioVolumeDown: 'VolumeDown',
        BrowserSearch: 'AC Search', BrowserHome: 'AC Home', BrowserBack: 'AC Back',
        BrowserForward: 'AC Forward', BrowserStop: 'AC Stop', BrowserRefresh: 'AC Refresh',
        BrowserFavorites: 'AC Bookmarks'
    };
    for (const [code, value] of Object.entries(keys)) {
        const captured = captureKey({code, key: 'ignored'});
        assert.deepEqual(captured, {value}, code);
        const ini = serialize([binding('Cross', 'KEY', captured.value)]);
        assert.deepEqual(parseIni(ini).entries, [binding('Cross', 'KEY', value)], code);
    }
});

test('capture rejects missing identifiers, composition and unsafe punctuation', () => {
    for (const event of [{code: 'Unidentified'}, {key: 'a'}, {code: 'IntlBackslash'}, {code: 'Semicolon'}, {code: 'KeyA', isComposing: true}, {code: 'Quote', key: 'Dead'}, {code: 'KeyA', key: 'Process'}]) {
        assert.ok(captureKey(event).error);
    }
});
