export const groups = [
    {name: 'Face buttons', actions: ['Cross', 'Circle', 'Triangle', 'Square']},
    {name: 'Shoulders, triggers and stick clicks', actions: ['L1', 'R1', 'L2', 'R2', 'L3', 'R3']},
    {name: 'Directional controls', actions: ['Up', 'Right', 'Down', 'Left']},
    {name: 'Left analog stick', actions: ['LeftStickLeft', 'LeftStickRight', 'LeftStickUp', 'LeftStickDown']},
    {name: 'Right analog stick', actions: ['RightStickLeft', 'RightStickRight', 'RightStickUp', 'RightStickDown']},
    {name: 'Touch and utility', actions: ['Options', 'TouchLeft', 'TouchRight', 'StickModifier', 'Shake', 'ToggleMouse', 'ToggleFullscreen', 'TogglePerformanceOverlay', 'Screenshot', 'ShowPadStatus']}
];

export const actions = groups.flatMap(group => group.actions);
export const padButtons = new Set(['Cross', 'Circle', 'Triangle', 'Square', 'L1', 'R1', 'L2', 'R2', 'L3', 'R3', 'Options', 'Up', 'Right', 'Down', 'Left']);
export const mouseValues = ['Left', 'Middle', 'Right', 'X1', 'X2'];
export const wheelValues = ['Up', 'Down'];

export const defaults = {
    Cross: ['KEY:Return', 'KEY:Space'], Circle: ['KEY:C'], Triangle: ['KEY:I'], Square: ['MOUSE:Left'],
    L1: ['KEY:Q'], R1: ['KEY:E', 'KEY:Left Alt', 'KEY:Right Alt'], L2: ['KEY:R', 'MOUSE:X1'], R2: ['MOUSE:Right'],
    L3: ['KEY:Left Shift', 'KEY:Right Shift'], R3: ['KEY:Left Ctrl', 'KEY:Right Ctrl'], Options: ['KEY:Escape'],
    Up: ['KEY:Up', 'WHEEL:Up'], Right: ['KEY:Right'], Down: ['KEY:Down', 'WHEEL:Down'], Left: ['KEY:Left'],
    LeftStickLeft: ['KEY:A'], LeftStickRight: ['KEY:D'], LeftStickUp: ['KEY:W'], LeftStickDown: ['KEY:S'],
    RightStickLeft: ['KEY:F'], RightStickRight: ['KEY:H'], RightStickUp: ['KEY:T'], RightStickDown: ['KEY:G'],
    TouchLeft: ['KEY:Backspace'], TouchRight: ['KEY:Tab'], StickModifier: [], Shake: ['KEY:X'],
    ToggleMouse: ['KEY:F7', 'MOUSE:Middle'], ToggleFullscreen: ['KEY:F11'], TogglePerformanceOverlay: ['KEY:F10'], Screenshot: ['KEY:F12'], ShowPadStatus: ['KEY:F9']
};

export function typesForAction(action) {
    if (!actions.includes(action)) return [];
    if (['ToggleFullscreen', 'TogglePerformanceOverlay', 'Screenshot', 'ShowPadStatus'].includes(action)) return ['KEY'];
    return padButtons.has(action) ? ['KEY', 'MOUSE', 'WHEEL'] : ['KEY', 'MOUSE'];
}

export function canonicalValue(value, choices) {
    if (typeof value !== 'string') return undefined;
    return choices.find(choice => choice.toUpperCase() === value.trim().toUpperCase());
}
