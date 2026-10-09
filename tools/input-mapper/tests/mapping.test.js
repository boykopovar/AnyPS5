import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {actions, defaults, mouseValues, padButtons, typesForAction} from '../mapping.js';
import {exportableKeys, keyNames} from '../keys.js';
import {captureKey} from '../capture.js';
import {parseIni, serialize, validate} from '../validation.js';

const binding = (action, type = 'KEY', value = 'F') => ({action, type, value});

test('action names and pad button categories agree with the current native parser', () => {
    const source = readFileSync(new URL('../../../core/libs/prx/libSceVideoOut/src/InputMapping.cpp', import.meta.url), 'utf8');
    const definitions = [...source.matchAll(/InputAction\{"(\w+)", Pad::InputControl::(\w+),/gu)];
    assert.deepEqual([...actions].sort(), definitions.map(match => match[1]).sort());
    assert.deepEqual([...padButtons].sort(), definitions.filter(match => match[2] === 'Button').map(match => match[1]).sort());
    assert.match(source, /SDL_GetScancodeFromName\(keyName\.c_str\(\)\)/u);
    assert.match(source, /if \(action\.control == Pad::InputControl::ToggleFullscreen\)/u);
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
            assert.equal(validate([binding(action, 'MOUSE', value)]).errors.length === 0, action !== 'ToggleFullscreen');
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

test('capture rejects missing identifiers, composition and unsafe punctuation', () => {
    for (const event of [{code: 'Unidentified'}, {key: 'a'}, {code: 'IntlBackslash'}, {code: 'Semicolon'}, {code: 'KeyA', isComposing: true}, {code: 'Quote', key: 'Dead'}, {code: 'KeyA', key: 'Process'}]) {
        assert.ok(captureKey(event).error);
    }
});
