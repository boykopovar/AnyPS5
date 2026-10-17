import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startInput } from '../input.mjs';
import { InputHeaderBytes, InputSlotBytes, InputSlots } from '../input-ring.mjs';
import { PadButton } from '../input-map.mjs';

class FakeWindow extends EventTarget {
  constructor() {
    super();
    this.callbacks = [];
  }

  requestAnimationFrame(callback) {
    this.callbacks.push(callback);
    return this.callbacks.length;
  }

  frame() {
    const callbacks = this.callbacks;
    this.callbacks = [];
    for (const callback of callbacks) callback(0);
  }
}

function keyEvent(type, code, repeat = false) {
  return Object.assign(new Event(type, { cancelable: true }), { code, repeat });
}

function attached(gamepads = () => []) {
  const window = new FakeWindow();
  const input = startInput(window, gamepads);
  const buffer = new SharedArrayBuffer(4096);
  const address = 256;
  input.attach(buffer, address);
  const header = new Uint32Array(buffer, address, 2);
  const slots = new DataView(buffer, address + InputHeaderBytes, InputSlots * InputSlotBytes);
  const buttons = (index) => slots.getUint32((index % InputSlots) * InputSlotBytes, true);
  return { window, header, buttons };
}

test('attaching sends the current state', () => {
  const { header, buttons } = attached();
  assert.equal(header[0], 1);
  assert.equal(buttons(0), 0);
});

test('a key press and the blur that follows reach the ring in order', () => {
  const { window, header, buttons } = attached();
  window.dispatchEvent(keyEvent('keydown', 'Enter'));
  window.dispatchEvent(new Event('blur'));
  assert.equal(header[0], 3);
  assert.equal(buttons(1), PadButton.Cross);
  assert.equal(buttons(2), 0);
});

test('a key release reaches the ring', () => {
  const { window, header, buttons } = attached();
  window.dispatchEvent(keyEvent('keydown', 'ArrowLeft'));
  window.dispatchEvent(keyEvent('keyup', 'ArrowLeft'));
  assert.equal(header[0], 3);
  assert.equal(buttons(1), PadButton.Left);
  assert.equal(buttons(2), 0);
});

test('auto-repeat adds no slots', () => {
  const { window, header } = attached();
  window.dispatchEvent(keyEvent('keydown', 'ArrowRight'));
  for (let index = 0; index < 5; index += 1) window.dispatchEvent(keyEvent('keydown', 'ArrowRight', true));
  window.frame();
  assert.equal(header[0], 2);
});

test('game keys keep the browser from acting on them', () => {
  const { window } = attached();
  const tab = keyEvent('keydown', 'Tab');
  const space = keyEvent('keyup', 'Space');
  const reload = keyEvent('keydown', 'F5');
  window.dispatchEvent(tab);
  window.dispatchEvent(space);
  window.dispatchEvent(reload);
  assert.ok(tab.defaultPrevented);
  assert.ok(space.defaultPrevented);
  assert.ok(!reload.defaultPrevented);
});

test('a full ring keeps the latest state until the runtime catches up', () => {
  const { window, header, buttons } = attached();
  Atomics.store(header, 0, InputSlots);
  window.dispatchEvent(keyEvent('keydown', 'ArrowUp'));
  window.dispatchEvent(keyEvent('keydown', 'Enter'));
  assert.equal(header[0], InputSlots);
  Atomics.store(header, 1, InputSlots);
  window.frame();
  assert.equal(header[0], InputSlots + 1);
  assert.equal(buttons(InputSlots), PadButton.Up | PadButton.Cross);
});

test('gamepads are read every frame', () => {
  const pad = { id: 'Xbox Wireless Controller', connected: true, axes: [0, 0, 0, 0], buttons: Array.from({ length: 17 }, () => ({ pressed: false, value: 0 })) };
  const { window, header, buttons } = attached(() => [null, pad]);
  pad.buttons[1] = { pressed: true, value: 1 };
  window.frame();
  assert.equal(header[0], 2);
  assert.equal(buttons(1), PadButton.Circle);
});

test('polling keeps running before the runtime attaches', () => {
  const window = new FakeWindow();
  startInput(window, () => []);
  window.dispatchEvent(keyEvent('keydown', 'Enter'));
  window.frame();
  assert.equal(window.callbacks.length, 1);
});
