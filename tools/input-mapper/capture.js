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
    NumpadBackspace: 'Keypad Backspace', NumpadClear: 'Keypad Clear', NumpadClearEntry: 'Keypad ClearEntry',
    NumpadParenLeft: 'Keypad (', NumpadParenRight: 'Keypad )',
    NumpadMemoryAdd: 'Keypad MemAdd', NumpadMemoryClear: 'Keypad MemClear',
    NumpadMemoryRecall: 'Keypad MemRecall', NumpadMemoryStore: 'Keypad MemStore',
    NumpadMemorySubtract: 'Keypad MemSubtract',
    ControlLeft: 'Left Ctrl', ControlRight: 'Right Ctrl', ShiftLeft: 'Left Shift', ShiftRight: 'Right Shift',
    AltLeft: 'Left Alt', AltRight: 'Right Alt', MetaLeft: 'Left GUI', MetaRight: 'Right GUI',
    MediaTrackNext: 'AudioNext', MediaTrackPrevious: 'AudioPrev', MediaStop: 'AudioStop',
    MediaPlayPause: 'AudioPlay', MediaSelect: 'MediaSelect',
    AudioVolumeMute: 'Mute', AudioVolumeUp: 'VolumeUp', AudioVolumeDown: 'VolumeDown',
    BrowserSearch: 'AC Search', BrowserHome: 'AC Home', BrowserBack: 'AC Back',
    BrowserForward: 'AC Forward', BrowserStop: 'AC Stop', BrowserRefresh: 'AC Refresh',
    BrowserFavorites: 'AC Bookmarks'
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
