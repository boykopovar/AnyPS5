import {exportableKeys} from './keys.js';

const codes = {
    Enter: 'Return', Escape: 'Escape', Space: 'Space', Backspace: 'Backspace', Tab: 'Tab',
    Minus: '-', Equal: '=', BracketLeft: '[', BracketRight: ']', Backslash: '\\',
    Semicolon: ';', Quote: "'", Backquote: '`', Comma: ',', Period: '.', Slash: '/',
    CapsLock: 'CapsLock', PrintScreen: 'PrintScreen', ScrollLock: 'ScrollLock', Pause: 'Pause',
    Insert: 'Insert', Home: 'Home', PageUp: 'PageUp', Delete: 'Delete', End: 'End', PageDown: 'PageDown',
    ArrowRight: 'Right', ArrowLeft: 'Left', ArrowDown: 'Down', ArrowUp: 'Up', NumLock: 'Numlock',
    NumpadDivide: 'Keypad /', NumpadMultiply: 'Keypad *', NumpadSubtract: 'Keypad -',
    NumpadAdd: 'Keypad +', NumpadEnter: 'Keypad Enter', NumpadDecimal: 'Keypad .',
    NumpadEqual: 'Keypad =', NumpadComma: 'Keypad ,', ContextMenu: 'Application',
    ControlLeft: 'Left Ctrl', ControlRight: 'Right Ctrl', ShiftLeft: 'Left Shift', ShiftRight: 'Right Shift',
    AltLeft: 'Left Alt', AltRight: 'Right Alt', MetaLeft: 'Left GUI', MetaRight: 'Right GUI'
};

export function captureKey(event) {
    if (event.isComposing || event.key === 'Dead' || event.key === 'Process') {
        return {error: 'Composition and dead keys are ambiguous. Select an SDL key from the list.'};
    }
    let value = codes[event.code];
    if (/^Key[A-Z]$/u.test(event.code)) value = event.code.slice(3);
    else if (/^Digit[0-9]$/u.test(event.code)) value = event.code.slice(5);
    else if (/^Numpad[0-9]$/u.test(event.code)) value = `Keypad ${event.code.slice(6)}`;
    else if (/^F(?:[1-9]|1[0-9]|2[0-4])$/u.test(event.code)) value = event.code;
    if (!value || !exportableKeys.includes(value)) {
        return {error: 'This physical key cannot be captured in the current INI format. Select an SDL key from the list.'};
    }
    return {value};
}
