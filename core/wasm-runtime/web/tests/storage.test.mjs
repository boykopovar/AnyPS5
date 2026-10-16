import { test } from 'node:test';
import assert from 'node:assert/strict';
import { openStorage } from '../storage.mjs';

class FakeFs {
  constructor() {
    this.directories = [];
    this.mounts = [];
    this.syncs = [];
    this.callbacks = [];
  }

  mkdir(path) {
    this.directories.push(path);
  }

  mount(type, options, path) {
    this.mounts.push({ type, path });
  }

  syncfs(populate, callback) {
    this.syncs.push(populate);
    this.callbacks.push(callback);
  }

  finish(error = null) {
    this.callbacks.shift()(error);
  }
}

const paths = ['/_sd', '/_sd_mem'];

function open(fs, idbfs = {}) {
  const lines = [];
  const events = [];
  const storage = openStorage(fs, idbfs, paths, { add: (id) => events.push('add ' + id), remove: (id) => events.push('remove ' + id) }, (text) => lines.push(text));
  return { storage, lines, events };
}

test('mounting loads every save directory before the game starts', () => {
  const fs = new FakeFs();
  const idbfs = {};
  const { lines, events } = open(fs, idbfs);
  assert.deepEqual(fs.directories, paths);
  assert.deepEqual(fs.mounts, paths.map((path) => ({ type: idbfs, path })));
  assert.deepEqual(fs.syncs, [true]);
  assert.deepEqual(events, ['add aps5-storage']);
  fs.finish();
  assert.deepEqual(events, ['add aps5-storage', 'remove aps5-storage']);
  assert.deepEqual(lines, []);
});

test('a failed load is logged and the game still starts', () => {
  const fs = new FakeFs();
  const { lines, events } = open(fs);
  fs.finish(new Error('blocked'));
  assert.deepEqual(events, ['add aps5-storage', 'remove aps5-storage']);
  assert.deepEqual(lines, ['anyps5 storage: blocked']);
});

test('commits wait until the stored saves are loaded', () => {
  const fs = new FakeFs();
  const { storage } = open(fs);
  storage.commit();
  assert.deepEqual(fs.syncs, [true]);
  fs.finish();
  assert.deepEqual(fs.syncs, [true, false]);
});

test('a failed load refuses commits so the stored saves stay intact', () => {
  const fs = new FakeFs();
  const { storage, lines } = open(fs);
  storage.commit();
  fs.finish(new Error('blocked'));
  storage.commit();
  assert.deepEqual(fs.syncs, [true]);
  assert.deepEqual(lines, ['anyps5 storage: blocked', 'anyps5 storage: not saved, the stored saves were not loaded', 'anyps5 storage: not saved, the stored saves were not loaded']);
});

test('commits run one at a time and a request during a commit runs once more', () => {
  const fs = new FakeFs();
  const { storage, lines } = open(fs);
  fs.finish();
  storage.commit();
  storage.commit();
  storage.commit();
  assert.deepEqual(fs.syncs, [true, false]);
  fs.finish();
  assert.deepEqual(fs.syncs, [true, false, false]);
  fs.finish();
  assert.deepEqual(fs.syncs, [true, false, false]);
  assert.deepEqual(lines, ['anyps5 storage: saved', 'anyps5 storage: saved']);
});

test('a failed commit is logged and the next one still runs', () => {
  const fs = new FakeFs();
  const { storage, lines } = open(fs);
  fs.finish();
  storage.commit();
  fs.finish(new Error('quota exceeded'));
  storage.commit();
  fs.finish();
  assert.deepEqual(lines, ['anyps5 storage: quota exceeded', 'anyps5 storage: saved']);
});
