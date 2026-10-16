import { test } from 'node:test';
import assert from 'node:assert/strict';
import { InputRingWriter, InputSlots, InputSlotBytes, InputHeaderBytes } from '../input-ring.mjs';

const neutral = { buttons: 0, sticks: [128, 128, 128, 128], l2: 0, r2: 0, touch: 0, device: 0 };
const address = 64;

function ring() {
  const buffer = new SharedArrayBuffer(4096);
  return { buffer, writer: new InputRingWriter(buffer, address), header: new Uint32Array(buffer, address, 2) };
}

function slotBytes(buffer, index) {
  return [...new Uint8Array(buffer, address + InputHeaderBytes + index * InputSlotBytes, InputSlotBytes)];
}

test('writes the slot layout the runtime reads', () => {
  const { buffer, writer, header } = ring();
  assert.ok(writer.push({ buttons: 0x104010, sticks: [1, 2, 3, 4], l2: 5, r2: 6, touch: 4, device: 1 }));
  assert.deepEqual([header[0], header[1]], [1, 0]);
  assert.deepEqual(slotBytes(buffer, 0), [0x10, 0x40, 0x10, 0x00, 1, 2, 3, 4, 5, 6, 4, 1]);
});

test('refuses to overwrite unread slots', () => {
  const { buffer, writer, header } = ring();
  for (let index = 0; index < InputSlots; index += 1) assert.ok(writer.push(neutral));
  assert.ok(!writer.push({ ...neutral, buttons: 9 }));
  assert.equal(header[0], InputSlots);
  Atomics.store(header, 1, 1);
  assert.ok(writer.push({ ...neutral, buttons: 9 }));
  assert.equal(slotBytes(buffer, 0)[0], 9);
});

test('indexes wrap around the 32-bit range', () => {
  const { buffer, writer, header } = ring();
  Atomics.store(header, 0, 0xffffffff);
  Atomics.store(header, 1, 0xffffffff);
  assert.ok(writer.push({ ...neutral, buttons: 7 }));
  assert.equal(header[0], 0);
  assert.equal(slotBytes(buffer, InputSlots - 1)[0], 7);
});
