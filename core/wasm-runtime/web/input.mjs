import { composePadState, samePadState, GameKeyCodes } from './input-map.mjs';
import { InputRingWriter } from './input-ring.mjs';

function connectedPads() {
  return typeof navigator.getGamepads === 'function' ? navigator.getGamepads() : [];
}

export function startInput(target, gamepads = connectedPads) {
  const pressed = new Set();
  let writer = null;
  let last = null;
  let pending = null;
  const drain = () => {
    if (writer !== null && pending !== null && writer.push(pending)) pending = null;
  };
  const update = () => {
    const pad = Array.from(gamepads()).find((candidate) => candidate !== null && candidate.connected) ?? null;
    const state = composePadState(pressed, pad);
    if (last === null || !samePadState(last, state)) {
      last = state;
      pending = state;
    }
    drain();
  };
  const frame = () => {
    update();
    target.requestAnimationFrame(frame);
  };
  target.addEventListener('keydown', (event) => {
    if (GameKeyCodes.has(event.code)) event.preventDefault();
    if (event.repeat) return;
    pressed.add(event.code);
    update();
  });
  target.addEventListener('keyup', (event) => {
    if (GameKeyCodes.has(event.code)) event.preventDefault();
    pressed.delete(event.code);
    update();
  });
  target.addEventListener('blur', () => {
    pressed.clear();
    update();
  });
  target.requestAnimationFrame(frame);
  return {
    attach(buffer, address) {
      writer = new InputRingWriter(buffer, address);
      update();
    },
  };
}
