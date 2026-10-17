export const InputSlots = 64;
export const InputSlotBytes = 12;
export const InputHeaderBytes = 8;

const Write = 0;
const Read = 1;

export class InputRingWriter {
  constructor(buffer, address) {
    this.header = new Int32Array(buffer, address, 2);
    this.slots = new DataView(buffer, address + InputHeaderBytes, InputSlots * InputSlotBytes);
  }

  push(state) {
    const write = Atomics.load(this.header, Write);
    const read = Atomics.load(this.header, Read);
    if ((write - read) >>> 0 >= InputSlots) return false;
    const offset = ((write >>> 0) % InputSlots) * InputSlotBytes;
    this.slots.setUint32(offset, state.buttons >>> 0, true);
    state.sticks.forEach((value, index) => this.slots.setUint8(offset + 4 + index, value));
    this.slots.setUint8(offset + 8, state.l2);
    this.slots.setUint8(offset + 9, state.r2);
    this.slots.setUint8(offset + 10, state.touch);
    this.slots.setUint8(offset + 11, state.device);
    Atomics.store(this.header, Write, (write + 1) | 0);
    Atomics.notify(this.header, Write);
    return true;
  }
}
