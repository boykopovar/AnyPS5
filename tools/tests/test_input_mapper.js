const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const root = path.resolve(__dirname, '..', '..');
const read = (...parts) => fs.readFileSync(path.join(root, ...parts), 'utf8');
const InputMapper = vm.runInThisContext(read('tools', 'input-mapper', 'mapper.js') + '\nInputMapper;');

const sdlHeader = read('3rdparty', 'SDL2', 'include', 'SDL_scancode.h');
const sdlIds = new Map();
for (const match of sdlHeader.matchAll(/SDL_SCANCODE_(\w+)\s*=\s*(\d+)/g)) {
    if (!sdlIds.has(match[1])) sdlIds.set(match[1], Number(match[2]));
}
const sdlKeyboard = read('3rdparty', 'SDL2', 'src', 'events', 'SDL_keyboard.c');
const tableStart = sdlKeyboard.indexOf('SDL_scancode_names[SDL_NUM_SCANCODES]');
const sdlNames = [];
for (const match of sdlKeyboard.slice(sdlKeyboard.indexOf('{', tableStart) + 1, sdlKeyboard.indexOf('};', tableStart))
    .matchAll(/\/\*\s*(\d+)\s*\*\/\s*(NULL|"((?:[^"\\]|\\.)*)")/g)) {
    if (match[2] !== 'NULL') sdlNames.push([Number(match[1]), JSON.parse(`"${match[3]}"`)]);
}

const parser = read('core', 'libs', 'prx', 'libSceVideoOut', 'src', 'InputMapping.cpp');
const cppActions = [...parser.matchAll(/InputAction\{"(\w+)", Pad::InputControl::(\w+), Pad::PadButton::(\w+)\}/g)]
    .map((match) => ({ name: match[1], control: match[2], button: match[3] }));
const defaultsHeader = read('core', 'libs', 'prx', 'libScePad', 'include', 'InputMapping.hpp');
const types = read('core', 'libs', 'prx', 'libScePad', 'include', 'PadInputTypes.hpp');

function names(bindings, action) {
    return bindings.get(action).map(InputMapper.format);
}

function check(text, expected, errors = []) {
    const result = InputMapper.parse(text);
    assert.deepEqual(result.errors, errors, JSON.stringify(text));
    for (const [action, sources] of Object.entries(expected)) {
        assert.deepEqual(names(result.bindings, action), sources, `${JSON.stringify(text)}: ${action}`);
    }
    return result;
}

function checkSdlTables() {
    assert.ok(sdlNames.length > 200);
    assert.deepEqual(InputMapper.scancodes.map(([key, , name]) => [key, name]), sdlNames);
    for (const [key, id] of InputMapper.scancodes) assert.equal(sdlIds.get(id), key, id);
    for (const [code, id] of Object.entries(InputMapper.capturedKeys)) assert.ok(sdlIds.has(id), `${code}: ${id}`);
}

function checkActionsAndDefaults() {
    assert.equal(cppActions.length, (parser.match(/InputAction\{/g) || []).length);
    assert.deepEqual(InputMapper.actions, cppActions.map((action) => ({ name: action.name, button: action.control === 'Button' })));

    const bindingCount = (defaultsHeader.match(/InputBinding\{/g) || []).length;
    const expected = [...defaultsHeader.matchAll(/InputBinding\{SDL_SCANCODE_(\w+), MouseButton::(\w+), InputControl::(\w+)(?:, PadButton::(\w+))?(?:, (-?\d+))?\}/g)]
        .map((match) => {
            const action = cppActions.find((candidate) => match[3] === 'Button'
                ? candidate.control === 'Button' && candidate.button === match[4]
                : candidate.control === match[3]);
            assert.ok(action, match[0]);
            const key = sdlIds.get(match[1]);
            if (match[1] !== 'UNKNOWN') return [action.name, 'KEY:' + sdlNames.find(([number]) => number === key)[1]];
            if (match[2] !== 'None') return [action.name, 'MOUSE:' + match[2]];
            return [action.name, 'WHEEL:' + (Number(match[5]) > 0 ? 'Up' : 'Down')];
        });
    assert.equal(expected.length, bindingCount);
    assert.deepEqual(InputMapper.builtIn, expected);

    const mouseEnum = types.slice(types.indexOf('enum class MouseButton'));
    const enumButtons = [...mouseEnum.slice(0, mouseEnum.indexOf('};')).matchAll(/^\s*(\w+) = SDL_BUTTON_/gm)].map((match) => match[1]);
    assert.deepEqual(InputMapper.mouseButtons, enumButtons);
    for (const button of InputMapper.mouseButtons) assert.ok(parser.includes(`"${button.toUpperCase()}"`), button);

    const reasons = [
        'expected Action = TYPE:VALUE',
        'unknown action \'',
        'source is empty',
        'source must use TYPE:VALUE format',
        'source value is empty',
        'unknown SDL key name \'',
        'ToggleFullscreen can only use a keyboard key',
        'unknown mouse button \'',
        'mouse wheel can only map to a pad button',
        'mouse wheel direction must be UP or DOWN',
        'source type must be KEY, MOUSE or WHEEL'
    ];
    for (const reason of reasons) assert.ok(parser.includes(reason), reason);
}

function checkParsing() {
    const builtIn = InputMapper.defaults();
    const result = check('', {});
    for (const action of InputMapper.actions) assert.deepEqual(result.bindings.get(action.name), builtIn.get(action.name));

    check('# comment\n\n   \t\n; comment\n', { Cross: ['KEY:Return', 'KEY:Space'] });
    check('Cross = KEY:F\n', { Cross: ['KEY:F'], Circle: ['KEY:C'] });
    check('Cross = KEY:F\nCross = KEY:G\n', { Cross: ['KEY:F', 'KEY:G'] });
    check('Cross = KEY:F\ncross = key:f\nCROSS=KEY:F', { Cross: ['KEY:F'] });
    check('Cross = KEY:Space\n', { Cross: ['KEY:Space'] });
    check('﻿Cross = KEY:F\r\nCircle = KEY:G\r\n', { Cross: ['KEY:F'], Circle: ['KEY:G'] });
    check('  Cross\t=\tKEY : left shift  # jump\n', { Cross: ['KEY:Left Shift'] });
    check('Cross = KEY:F ; comment\n', { Cross: ['KEY:F'] });
    check('Up = KEY:W\n', { Up: ['KEY:W'] });
    check('Square = MOUSE:x2\nR2 = mouse:middle\n', { Square: ['MOUSE:X2'], R2: ['MOUSE:Middle'] });
    check('L2 = WHEEL:down\nUp = KEY:Up\n', { L2: ['WHEEL:Down'], Up: ['KEY:Up'], Down: ['KEY:Down', 'WHEEL:Down'] });
    check('Cross = KEY:=\nCircle = KEY:Keypad =\nTriangle = KEY:Keypad :\n',
        { Cross: ['KEY:='], Circle: ['KEY:Keypad ='], Triangle: ['KEY:Keypad :'] });
    check('Cross = KEY:return\n', { Cross: ['KEY:Return'] });
    assert.equal(InputMapper.parse('Cross = KEY:return\n').bindings.get('Cross')[0].key, 40);

    check([
        'Crss = KEY:A',
        'Cross KEY:A',
        'Cross =',
        'Cross = A',
        'Cross = KEY:',
        'Cross = KEY:Nope',
        'ToggleFullscreen = MOUSE:Left',
        'LeftStickUp = WHEEL:Up',
        'Up = WHEEL:Left',
        'Up = PAD:A',
        'Square = MOUSE:X3',
        'Cross = KEY:;',
        'Cross = KEY:Keypad #',
        '﻿Circle = KEY:A',
        'Cross = KEY:Space ',
        'ToggleFullſcreen = KEY:A',
        ' = KEY:A'
    ].join('\n'), { Cross: ['KEY:Return', 'KEY:Space'], Circle: ['KEY:C'] }, [
        { line: 1, reason: 'unknown action \'Crss\'' },
        { line: 2, reason: 'expected Action = TYPE:VALUE' },
        { line: 3, reason: 'source is empty' },
        { line: 4, reason: 'source must use TYPE:VALUE format' },
        { line: 5, reason: 'source value is empty' },
        { line: 6, reason: 'unknown SDL key name \'Nope\'' },
        { line: 7, reason: 'ToggleFullscreen can only use a keyboard key' },
        { line: 8, reason: 'mouse wheel can only map to a pad button' },
        { line: 9, reason: 'mouse wheel direction must be UP or DOWN' },
        { line: 10, reason: 'source type must be KEY, MOUSE or WHEEL' },
        { line: 11, reason: 'unknown mouse button \'X3\'' },
        { line: 12, reason: 'source value is empty' },
        { line: 13, reason: 'unknown SDL key name \'Keypad\'' },
        { line: 14, reason: 'unknown action \'﻿Circle\'' },
        { line: 15, reason: 'unknown SDL key name \'Space \'' },
        { line: 16, reason: 'unknown action \'ToggleFullſcreen\'' },
        { line: 17, reason: 'unknown action \'\'' }
    ]);
}

function every() {
    const sources = InputMapper.scancodes.map(([key]) => ({ type: 'KEY', key }));
    for (const button of InputMapper.mouseButtons) sources.push({ type: 'MOUSE', button });
    sources.push({ type: 'WHEEL', direction: 'Up' }, { type: 'WHEEL', direction: 'Down' });
    return sources;
}

function checkRoundTrip() {
    const unwritable = new Set(['#', ';', 'Keypad #']);
    let written = 0;
    for (const action of InputMapper.actions) {
        for (const binding of every()) {
            const problem = InputMapper.problem(action.name, binding);
            const name = binding.type === 'KEY' ? InputMapper.keyName(binding.key) : '';
            const expectedProblem = (binding.type === 'KEY' && (unwritable.has(name) || binding.key === 158)) ||
                (binding.type !== 'KEY' && action.name === 'ToggleFullscreen') ||
                (binding.type === 'WHEEL' && !action.button);
            assert.equal(problem !== '', expectedProblem, `${action.name} ${InputMapper.format(binding)}: ${problem}`);
            if (problem) continue;
            const bindings = InputMapper.defaults();
            bindings.set(action.name, [binding]);
            const text = InputMapper.serialize(bindings);
            const result = InputMapper.parse(text);
            assert.deepEqual(result.errors, [], text);
            for (const other of InputMapper.actions) {
                assert.deepEqual(result.bindings.get(other.name), bindings.get(other.name), `${text}: ${other.name}`);
            }
            ++written;
        }
    }
    assert.ok(written > 5000);

    for (const code of ['IntlBackslash', 'IntlRo', 'IntlYen', 'KanaMode', 'Convert', 'NonConvert']) {
        const found = InputMapper.keyFromCode(code);
        assert.equal(found.key, 0, code);
        assert.notEqual(InputMapper.problem('Cross', { type: 'KEY', key: found.key }), '', code);
    }
    assert.deepEqual(InputMapper.keyFromCode('KeyA'), { id: 'A', key: 4 });
    assert.deepEqual(InputMapper.keyFromCode('Enter'), { id: 'RETURN', key: 40 });
    assert.deepEqual(InputMapper.keyFromCode('Numpad7'), { id: 'KP_7', key: 95 });
    assert.deepEqual(InputMapper.keyFromCode('AltRight'), { id: 'RALT', key: 230 });
    assert.deepEqual(InputMapper.keyFromCode('F24'), { id: 'F24', key: 115 });
    assert.equal(InputMapper.keyFromCode('LaunchApp1'), null);
    assert.equal(InputMapper.keyFromCode('toString'), null);
    return written;
}

function checkSerializing() {
    const bindings = InputMapper.defaults();
    assert.equal(InputMapper.serialize(bindings), '');
    assert.deepEqual(InputMapper.conflicts(bindings), []);
    bindings.set('Square', [...bindings.get('Square'), { type: 'KEY', key: 44 }]);
    bindings.set('Cross', [{ type: 'KEY', key: 44 }, { type: 'KEY', key: 40 }]);
    assert.equal(InputMapper.changed(bindings, 'Cross'), false);
    assert.equal(InputMapper.serialize(bindings), 'Square = MOUSE:Left\nSquare = KEY:Space\n');
    assert.deepEqual(InputMapper.conflicts(bindings).map((entry) => entry.actions), [['Cross', 'Square']]);
    bindings.set('Circle', []);
    assert.throws(() => InputMapper.serialize(bindings));
    bindings.set('Circle', [{ type: 'KEY', key: 6 }]);
    bindings.set('L2', [{ type: 'MOUSE', button: 'X1' }]);
    bindings.set('ToggleFullscreen', [{ type: 'KEY', key: 69 }]);
    assert.equal(InputMapper.serialize(bindings),
        'Square = MOUSE:Left\nSquare = KEY:Space\nL2 = MOUSE:X1\nToggleFullscreen = KEY:F12\n');
}

checkSdlTables();
checkActionsAndDefaults();
checkParsing();
const written = checkRoundTrip();
checkSerializing();
console.log(`PASS: ${InputMapper.scancodes.length} SDL key names, ${InputMapper.actions.length} actions, ` +
    `${InputMapper.builtIn.length} built-in bindings, ${written} written bindings read back`);
