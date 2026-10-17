export const AudioRingFrames = 8192;
export const AudioRings = 4;
export const AudioHeaderBytes = 32;
export const AudioRingBytes = AudioHeaderBytes + AudioRingFrames * 2 * 4;

const Running = 0;
const Active = 0;
const Write = 1;
const Read = 2;
const Underruns = 3;
const Consumed = 4;

export class AudioMixer {
  constructor(buffer, address) {
    this.state = new Uint32Array(buffer, address, AudioHeaderBytes / 4);
    this.rings = [];
    for (let index = 0; index < AudioRings; index += 1) {
      const base = address + AudioHeaderBytes + index * AudioRingBytes;
      this.rings.push({ header: new Uint32Array(buffer, base, AudioHeaderBytes / 4), samples: new Float32Array(buffer, base + AudioHeaderBytes, AudioRingFrames * 2) });
    }
  }

  mix(left, right) {
    left.fill(0);
    right.fill(0);
    for (const ring of this.rings) {
      if (Atomics.load(ring.header, Active) === 0) continue;
      const read = Atomics.load(ring.header, Read);
      const write = Atomics.load(ring.header, Write);
      const available = (write - read) >>> 0;
      if (available > AudioRingFrames) {
        Atomics.store(ring.header, Read, write);
        continue;
      }
      const frames = Math.min(available, left.length);
      let audible = 0;
      for (let frame = 0; frame < frames; frame += 1) {
        const slot = ((read + frame) % AudioRingFrames) * 2;
        const leftSample = ring.samples[slot];
        const rightSample = ring.samples[slot + 1];
        left[frame] += leftSample;
        right[frame] += rightSample;
        if (leftSample !== 0 || rightSample !== 0) audible += 1;
      }
      Atomics.store(ring.header, Read, (read + frames) >>> 0);
      if (audible > 0) Atomics.add(ring.header, Consumed, audible);
      if (frames < left.length) Atomics.add(ring.header, Underruns, 1);
    }
    for (let frame = 0; frame < left.length; frame += 1) {
      left[frame] = Math.max(-1, Math.min(1, left[frame]));
      right[frame] = Math.max(-1, Math.min(1, right[frame]));
    }
  }

  stats() {
    let active = 0;
    let consumed = 0;
    let underruns = 0;
    for (const ring of this.rings) {
      if (Atomics.load(ring.header, Active) !== 0) active += 1;
      consumed += Atomics.load(ring.header, Consumed);
      underruns += Atomics.load(ring.header, Underruns);
    }
    return { running: Atomics.load(this.state, Running), active, consumed, underruns };
  }
}
