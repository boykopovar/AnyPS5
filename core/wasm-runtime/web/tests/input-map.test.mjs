import { test } from 'node:test';
import assert from 'node:assert/strict';
import { composePadState, samePadState, stickByte, deviceKind, GameKeyCodes, PadButton, Touch, Device } from '../input-map.mjs';

const none = new Set();

function pad(id, pressed = [], values = {}, axes = [0, 0, 0, 0]) {
  const buttons = Array.from({ length: 18 }, (_, index) => ({ pressed: pressed.includes(index), value: values[index] ?? (pressed.includes(index) ? 1 : 0) }));
  return { id, buttons, axes, connected: true };
}

test('neutral state without input', () => {
  assert.deepEqual(composePadState(none, null), { buttons: 0, sticks: [128, 128, 128, 128], l2: 0, r2: 0, touch: 0, device: Device.None });
});

test('keyboard buttons follow the native default mapping', () => {
  const state = composePadState(new Set(['Enter', 'ArrowLeft', 'KeyQ', 'AltRight', 'Escape']), null);
  assert.equal(state.buttons, PadButton.Cross | PadButton.Left | PadButton.L1 | PadButton.R1 | PadButton.Options);
  assert.equal(composePadState(new Set(['Space']), null).buttons, PadButton.Cross);
});

test('keyboard sticks tilt fully and opposite keys cancel', () => {
  assert.deepEqual(composePadState(new Set(['KeyA', 'KeyS']), null).sticks, [0, 255, 128, 128]);
  assert.deepEqual(composePadState(new Set(['KeyA', 'KeyD', 'KeyT']), null).sticks, [128, 128, 128, 0]);
});

test('touch keys press the touch pad halves', () => {
  const state = composePadState(new Set(['Backspace', 'Tab']), null);
  assert.equal(state.touch, Touch.Left | Touch.Right);
  assert.equal(state.buttons, 0);
});

test('stick bytes match the native conversion', () => {
  assert.equal(stickByte(-32768), 0);
  assert.equal(stickByte(0), 128);
  assert.equal(stickByte(32767), 255);
});

test('standard gamepad buttons, triggers and axes', () => {
  const state = composePadState(none, pad('Xbox Wireless Controller (STANDARD GAMEPAD)', [0, 3, 9, 12], { 6: 0.5 }, [-1, 1, 0, 0]));
  assert.equal(state.buttons, PadButton.Cross | PadButton.Triangle | PadButton.Options | PadButton.Up | PadButton.L2);
  assert.equal(state.l2, 128);
  assert.equal(state.r2, 0);
  assert.deepEqual(state.sticks, [0, 255, 128, 128]);
  assert.equal(state.device, Device.Other);
});

test('view is the touch pad on other pads, the touch pad click on PlayStation pads', () => {
  const other = composePadState(none, pad('Xbox Wireless Controller', [8]));
  assert.equal(other.buttons, PadButton.TouchPad);
  assert.equal(other.touch, Touch.Center);
  const dualSense = composePadState(none, pad('DualSense Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 0ce6)', [8, 17]));
  assert.equal(dualSense.buttons, PadButton.TouchPad);
  assert.equal(dualSense.touch, 0);
  assert.equal(dualSense.device, Device.DualSense);
});

test('keyboard axes override the gamepad axes', () => {
  const state = composePadState(new Set(['KeyD']), pad('Xbox Wireless Controller', [], {}, [-1, 0.5, 0, 0]));
  assert.deepEqual(state.sticks, [255, stickByte(Math.round(0.5 * 32767)), 128, 128]);
});

test('device kinds come from the gamepad id', () => {
  assert.equal(deviceKind('Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)'), Device.DualShock4);
  assert.equal(deviceKind('054c-0ce6-DualSense Wireless Controller'), Device.DualSense);
  assert.equal(deviceKind('Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)'), Device.Other);
});

test('game keys are exactly the mapped keys', () => {
  for (const code of ['Enter', 'Space', 'Tab', 'Backspace', 'ArrowDown', 'KeyW', 'Escape']) assert.ok(GameKeyCodes.has(code), code);
  for (const code of ['F5', 'KeyZ', 'Digit1']) assert.ok(!GameKeyCodes.has(code), code);
});

test('samePadState compares every field', () => {
  const base = composePadState(none, null);
  assert.ok(samePadState(base, composePadState(none, null)));
  for (const change of [{ buttons: 1 }, { l2: 1 }, { r2: 1 }, { touch: 1 }, { device: 1 }, { sticks: [128, 128, 128, 0] }]) {
    assert.ok(!samePadState(base, { ...base, ...change }), JSON.stringify(change));
  }
});
