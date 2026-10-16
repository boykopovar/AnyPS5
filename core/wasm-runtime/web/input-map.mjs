export const PadButton = Object.freeze({
  L3: 0x2, R3: 0x4, Options: 0x8, Up: 0x10, Right: 0x20, Down: 0x40, Left: 0x80,
  L2: 0x100, R2: 0x200, L1: 0x400, R1: 0x800, Triangle: 0x1000, Circle: 0x2000, Cross: 0x4000, Square: 0x8000,
  TouchPad: 0x100000,
});

export const Touch = Object.freeze({ Left: 1, Right: 2, Center: 4 });

export const Device = Object.freeze({ None: 0, DualSense: 1, DualShock4: 2, Other: 3 });

const KeyButtons = new Map([
  ['Enter', PadButton.Cross], ['Space', PadButton.Cross], ['Escape', PadButton.Options],
  ['KeyI', PadButton.Triangle], ['KeyC', PadButton.Circle], ['KeyR', PadButton.Square], ['KeyQ', PadButton.L1], ['KeyE', PadButton.R1],
  ['AltLeft', PadButton.R1], ['AltRight', PadButton.R1], ['ShiftLeft', PadButton.L3], ['ShiftRight', PadButton.L3],
  ['ControlLeft', PadButton.R3], ['ControlRight', PadButton.R3],
  ['ArrowUp', PadButton.Up], ['ArrowRight', PadButton.Right], ['ArrowDown', PadButton.Down], ['ArrowLeft', PadButton.Left],
]);

const KeyAxes = new Map([
  ['KeyA', [0, -1]], ['KeyD', [0, 1]], ['KeyW', [1, -1]], ['KeyS', [1, 1]],
  ['KeyF', [2, -1]], ['KeyH', [2, 1]], ['KeyT', [3, -1]], ['KeyG', [3, 1]],
]);

const KeyTouch = new Map([['Backspace', Touch.Left], ['Tab', Touch.Right]]);

const GamepadButtons = [
  PadButton.Cross, PadButton.Circle, PadButton.Square, PadButton.Triangle, PadButton.L1, PadButton.R1, 0, 0, 0,
  PadButton.Options, PadButton.L3, PadButton.R3, PadButton.Up, PadButton.Down, PadButton.Left, PadButton.Right,
];

export const GameKeyCodes = new Set([...KeyButtons.keys(), ...KeyAxes.keys(), ...KeyTouch.keys()]);

export function stickByte(axis) {
  return Math.floor(((axis + 32768) * 255 + 32767) / 65535);
}

function axisByte(value) {
  return stickByte(Math.round(Math.max(-1, Math.min(1, value)) * 32767));
}

function triggerByte(value) {
  return Math.round(Math.max(0, Math.min(1, value)) * 255);
}

export function deviceKind(id) {
  const text = id.toLowerCase();
  const sony = text.includes('054c');
  if (text.includes('dualsense') || (sony && /0ce6|0df2/.test(text))) return Device.DualSense;
  if (text.includes('dualshock') || (sony && /05c4|09cc|0ba0/.test(text))) return Device.DualShock4;
  return Device.Other;
}

function pressedAt(gamepad, index) {
  return gamepad.buttons[index]?.pressed === true;
}

export function composePadState(pressed, gamepad) {
  const state = { buttons: 0, sticks: [128, 128, 128, 128], l2: 0, r2: 0, touch: 0, device: Device.None };
  if (gamepad !== null) {
    state.device = deviceKind(gamepad.id);
    GamepadButtons.forEach((button, index) => {
      if (button !== 0 && pressedAt(gamepad, index)) state.buttons |= button;
    });
    state.l2 = triggerByte(gamepad.buttons[6]?.value ?? 0);
    state.r2 = triggerByte(gamepad.buttons[7]?.value ?? 0);
    const playStation = state.device === Device.DualSense || state.device === Device.DualShock4;
    if (playStation && pressedAt(gamepad, 17)) state.buttons |= PadButton.TouchPad;
    if (!playStation && pressedAt(gamepad, 8)) {
      state.buttons |= PadButton.TouchPad;
      state.touch |= Touch.Center;
    }
    for (let axis = 0; axis < 4; axis += 1) state.sticks[axis] = axisByte(gamepad.axes[axis] ?? 0);
  }
  const negative = [false, false, false, false];
  const positive = [false, false, false, false];
  for (const code of pressed) {
    state.buttons |= KeyButtons.get(code) ?? 0;
    state.touch |= KeyTouch.get(code) ?? 0;
    const axis = KeyAxes.get(code);
    if (axis !== undefined) (axis[1] < 0 ? negative : positive)[axis[0]] = true;
  }
  for (let axis = 0; axis < 4; axis += 1) {
    if (negative[axis] || positive[axis]) state.sticks[axis] = negative[axis] === positive[axis] ? 128 : negative[axis] ? 0 : 255;
  }
  if (state.l2 !== 0) state.buttons |= PadButton.L2;
  if (state.r2 !== 0) state.buttons |= PadButton.R2;
  state.buttons >>>= 0;
  return state;
}

export function samePadState(first, second) {
  return first.buttons === second.buttons && first.l2 === second.l2 && first.r2 === second.r2 && first.touch === second.touch
    && first.device === second.device && first.sticks.every((value, index) => value === second.sticks[index]);
}
