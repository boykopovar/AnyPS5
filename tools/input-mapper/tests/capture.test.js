import test from 'node:test';
import assert from 'node:assert/strict';
import {captureKey} from '../capture.js';

const mockEvent = (code, key = undefined, isComposing = false) => ({
    code,
    key,
    isComposing
});

test('captureKey maps letter keys to SDL key names', () => {
    const result = captureKey(mockEvent('KeyA'));
    assert.deepEqual(result, {value: 'A'});
});

test('captureKey maps digit keys to SDL key names', () => {
    const result = captureKey(mockEvent('Digit1'));
    assert.deepEqual(result, {value: '1'});
});

test('captureKey maps numpad keys to SDL key names', () => {
    const result = captureKey(mockEvent('Numpad5'));
    assert.deepEqual(result, {value: 'Keypad 5'});
});

test('captureKey maps function keys to SDL key names', () => {
    const result = captureKey(mockEvent('F1'));
    assert.deepEqual(result, {value: 'F1'});
    const result12 = captureKey(mockEvent('F12'));
    assert.deepEqual(result12, {value: 'F12'});
});

test('captureKey maps special keys to SDL key names', () => {
    const cases = [
        ['Enter', 'Return'],
        ['Escape', 'Escape'],
        ['Space', 'Space'],
        ['Backspace', 'Backspace'],
        ['Tab', 'Tab'],
        ['Minus', '-'],
        ['Equal', '='],
        ['BracketLeft', '['],
        ['BracketRight', ']'],
        ['Backslash', '\\'],
        ['Semicolon', ';'],
        ['Quote', "'"],
        ['Backquote', '`'],
        ['Comma', ','],
        ['Period', '.'],
        ['Slash', '/'],
        ['CapsLock', 'CapsLock'],
        ['PrintScreen', 'PrintScreen'],
        ['ScrollLock', 'ScrollLock'],
        ['Pause', 'Pause'],
        ['Insert', 'Insert'],
        ['Home', 'Home'],
        ['PageUp', 'PageUp'],
        ['Delete', 'Delete'],
        ['End', 'End'],
        ['PageDown', 'PageDown'],
        ['ArrowRight', 'Right'],
        ['ArrowLeft', 'Left'],
        ['ArrowDown', 'Down'],
        ['ArrowUp', 'Up'],
        ['NumLock', 'Numlock'],
        ['NumpadDivide', 'Keypad /'],
        ['NumpadMultiply', 'Keypad *'],
        ['NumpadSubtract', 'Keypad -'],
        ['NumpadAdd', 'Keypad +'],
        ['NumpadEnter', 'Keypad Enter'],
        ['NumpadDecimal', 'Keypad .'],
        ['NumpadEqual', 'Keypad ='],
        ['NumpadComma', 'Keypad ,'],
        ['ContextMenu', 'Application'],
        ['ControlLeft', 'Left Ctrl'],
        ['ControlRight', 'Right Ctrl'],
        ['ShiftLeft', 'Left Shift'],
        ['ShiftRight', 'Right Shift'],
        ['AltLeft', 'Left Alt'],
        ['AltRight', 'Right Alt'],
        ['MetaLeft', 'Left GUI'],
        ['MetaRight', 'Right GUI'],
        ['MediaTrackNext', 'AudioNext'],
        ['MediaTrackPrevious', 'AudioPrev'],
        ['MediaStop', 'AudioStop'],
        ['MediaPlayPause', 'AudioPlay'],
        ['MediaSelect', 'MediaSelect'],
        ['AudioVolumeMute', 'Mute'],
        ['AudioVolumeUp', 'VolumeUp'],
        ['AudioVolumeDown', 'VolumeDown'],
        ['BrowserSearch', 'AC Search'],
        ['BrowserHome', 'AC Home'],
        ['BrowserBack', 'AC Back'],
        ['BrowserForward', 'AC Forward'],
        ['BrowserStop', 'AC Stop'],
        ['BrowserRefresh', 'AC Refresh'],
        ['BrowserFavorites', 'AC Bookmarks'],
    ];
    for (const [code, expected] of cases) {
        const result = captureKey(mockEvent(code));
        assert.deepEqual(result, {value: expected}, `code: ${code}`);
    }
});

test('captureKey rejects composition and dead keys', () => {
    const result = captureKey(mockEvent('KeyA', 'Dead', true));
    assert.ok(result.error);
    assert.match(result.error, /Composition and dead keys are ambiguous/);
});

test('captureKey rejects non-exportable keys', () => {
    const result = captureKey(mockEvent('UnknownKey'));
    assert.ok(result.error);
    assert.match(result.error, /This physical key cannot be captured/);
});

test('captureKey rejects F25 and above', () => {
    const result = captureKey(mockEvent('F25'));
    assert.ok(result.error);
});

test('captureKey rejects Numpad keys above 9', () => {
    const result = captureKey(mockEvent('Numpad10'));
    assert.ok(result.error);
});