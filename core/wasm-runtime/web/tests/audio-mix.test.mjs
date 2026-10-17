import { test } from 'node:test';
import assert from 'node:assert/strict';
import { AudioMixer, AudioRingFrames, AudioRings, AudioHeaderBytes, AudioRingBytes } from '../audio-mix.mjs';

const address = 128;

function setup() {
  const buffer = new SharedArrayBuffer(address + AudioHeaderBytes + AudioRings * AudioRingBytes);
  const ring = (index) => {
    const base = address + AudioHeaderBytes + index * AudioRingBytes;
    return { header: new Uint32Array(buffer, base, AudioHeaderBytes / 4), samples: new Float32Array(buffer, base + AudioHeaderBytes, AudioRingFrames * 2) };
  };
  return { buffer, ring, mixer: new AudioMixer(buffer, address) };
}

function produce(ring, frames) {
  const write = ring.header[1];
  frames.forEach(([left, right], index) => {
    const slot = ((write + index) % AudioRingFrames) * 2;
    ring.samples[slot] = left;
    ring.samples[slot + 1] = right;
  });
  ring.header[1] = (write + frames.length) >>> 0;
}

function output(frames) {
  return [new Float32Array(frames), new Float32Array(frames)];
}

test('the byte layout matches the runtime', () => {
  assert.equal(AudioHeaderBytes, 32);
  assert.equal(AudioRingBytes, 32 + 8192 * 8);
});

test('inactive rings are silent and untouched', () => {
  const { ring, mixer } = setup();
  produce(ring(0), [[0.5, 0.5]]);
  const [left, right] = output(4);
  left.fill(1);
  mixer.mix(left, right);
  assert.deepEqual([...left, ...right], [0, 0, 0, 0, 0, 0, 0, 0]);
  assert.equal(ring(0).header[2], 0);
  assert.equal(ring(0).header[3], 0);
});

test('mixes queued frames and advances the read index', () => {
  const { ring, mixer } = setup();
  ring(0).header[0] = 1;
  produce(ring(0), [[0.5, -0.5], [0, 0], [0.25, 0.75], [-0.25, 0]]);
  const [left, right] = output(4);
  mixer.mix(left, right);
  assert.deepEqual([...left], [0.5, 0, 0.25, -0.25]);
  assert.deepEqual([...right], [-0.5, 0, 0.75, 0]);
  assert.equal(ring(0).header[2], 4);
  assert.equal(ring(0).header[3], 0);
  assert.equal(ring(0).header[4], 3);
});

test('counts an underrun when a ring runs dry', () => {
  const { ring, mixer } = setup();
  ring(1).header[0] = 1;
  produce(ring(1), [[0.5, 0.5], [0.5, 0.5]]);
  const [left, right] = output(4);
  mixer.mix(left, right);
  assert.deepEqual([...left], [0.5, 0.5, 0, 0]);
  assert.equal(ring(1).header[2], 2);
  assert.equal(ring(1).header[3], 1);
});

test('sums rings and clamps', () => {
  const { ring, mixer } = setup();
  ring(0).header[0] = 1;
  ring(3).header[0] = 1;
  produce(ring(0), [[0.75, -0.75]]);
  produce(ring(3), [[0.5, -0.5]]);
  const [left, right] = output(1);
  mixer.mix(left, right);
  assert.deepEqual([left[0], right[0]], [1, -1]);
});

test('reads across the end of the ring', () => {
  const { ring, mixer } = setup();
  const target = ring(2);
  target.header[0] = 1;
  target.header[1] = AudioRingFrames - 1;
  target.header[2] = AudioRingFrames - 1;
  produce(target, [[0.25, 0.25], [0.5, 0.5]]);
  const [left, right] = output(2);
  mixer.mix(left, right);
  assert.deepEqual([...left], [0.25, 0.5]);
  assert.equal(target.header[2], AudioRingFrames + 1);
});

test('stats sum the ring counters', () => {
  const { buffer, ring, mixer } = setup();
  new Uint32Array(buffer, address, 1)[0] = 1;
  ring(0).header[0] = 1;
  ring(0).header[3] = 2;
  ring(0).header[4] = 10;
  ring(1).header[3] = 1;
  ring(1).header[4] = 5;
  assert.deepEqual(mixer.stats(), { running: 1, active: 1, consumed: 15, underruns: 3 });
});

test('a ring reopened while the worklet was reading resynchronises', () => {
  const { ring, mixer } = setup();
  const target = ring(0);
  target.header[0] = 1;
  target.header[1] = 1000;
  target.header[2] = 1128;
  const [left, right] = output(4);
  mixer.mix(left, right);
  assert.deepEqual([...left], [0, 0, 0, 0]);
  assert.equal(target.header[2], 1000);
  produce(target, [[0.5, 0.5]]);
  const [nextLeft, nextRight] = output(1);
  mixer.mix(nextLeft, nextRight);
  assert.equal(nextLeft[0], 0.5);
  assert.equal(target.header[2], 1001);
});
