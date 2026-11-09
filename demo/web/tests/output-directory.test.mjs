import assert from "node:assert/strict";
import { test } from "node:test";
import { abortAndDiscardOutput, OutputDirectoryTransaction } from "../output-directory.mjs";

function missingDirectory() {
  const error = new Error("not found");
  error.name = "NotFoundError";
  return error;
}

test("creates a new output root only after confirming the destination is absent", async () => {
  const directory = {};
  const calls = [];
  const parent = {
    async getDirectoryHandle(name, options) {
      calls.push([name, options]);
      if (!options) throw missingDirectory();
      return directory;
    },
    async removeEntry() {}
  };

  const transaction = await OutputDirectoryTransaction.create(parent, "app-linux");

  assert.equal(transaction.directory, directory);
  assert.deepEqual(calls, [["app-linux", undefined], ["app-linux", { create: true }]]);
});

test("does not claim or delete a destination that already exists", async () => {
  const parent = {
    async getDirectoryHandle() { return {}; },
    async removeEntry() { assert.fail("existing destination must not be removed"); }
  };

  await assert.rejects(
    OutputDirectoryTransaction.create(parent, "app-linux"),
    /already exists/u
  );
});

test("discards its incomplete output recursively", async () => {
  let removed;
  const parent = {
    async getDirectoryHandle(name, options) {
      if (!options) throw missingDirectory();
      return { name };
    },
    async removeEntry(...args) { removed = args; }
  };
  const transaction = await OutputDirectoryTransaction.create(parent, "app-linux");

  await transaction.discard();
  await transaction.discard();

  assert.deepEqual(removed, ["app-linux", { recursive: true }]);
});

test("keeps completed output and reports cleanup failures", async () => {
  let removeCount = 0;
  const parent = {
    async getDirectoryHandle(name, options) {
      if (!options) throw missingDirectory();
      return { name };
    },
    async removeEntry() {
      removeCount++;
      throw new Error("permission denied");
    }
  };
  const completed = await OutputDirectoryTransaction.create(parent, "complete-linux");
  completed.commit();
  await completed.discard();
  assert.equal(removeCount, 0);

  const failed = await OutputDirectoryTransaction.create(parent, "partial-linux");
  await assert.rejects(failed.discard(), /permission denied/u);
  assert.equal(removeCount, 1);
});

test("aborts an open file before removing the incomplete output tree", async () => {
  const calls = [];
  const parent = {
    async getDirectoryHandle(name, options) {
      if (!options) throw missingDirectory();
      return { name };
    },
    async removeEntry(name, options) { calls.push(["remove", name, options]); }
  };
  const transaction = await OutputDirectoryTransaction.create(parent, "partial-linux");
  let finishAbort;
  const sink = { abort() { calls.push(["abort"]); return new Promise(resolve => { finishAbort = resolve; }); } };

  const cleanup = abortAndDiscardOutput(transaction, sink);
  await Promise.resolve();
  assert.deepEqual(calls, [["abort"]]);
  finishAbort();
  assert.deepEqual(await cleanup, []);
  assert.deepEqual(calls, [["abort"], ["remove", "partial-linux", { recursive: true }]]);
});

test("returns sink and folder cleanup failures without hiding either", async () => {
  const parent = {
    async getDirectoryHandle(name, options) {
      if (!options) throw missingDirectory();
      return { name };
    },
    async removeEntry() { throw new Error("directory permission denied"); }
  };
  const transaction = await OutputDirectoryTransaction.create(parent, "partial-linux");
  const sink = { async abort() { throw new Error("file is busy"); } };

  assert.deepEqual(await abortAndDiscardOutput(transaction, sink), [
    "Could not discard the output file: file is busy",
    "Could not remove incomplete output folder partial-linux/: directory permission denied"
  ]);
});

test("concurrent cleanup shares one removal and prevents commit until it settles", { timeout: 3000 }, async () => {
  let finish;
  let calls = 0;
  const parent = { removeEntry() { calls++; return new Promise(resolve => { finish = resolve; }); } };
  const transaction = new OutputDirectoryTransaction(parent, "partial", {});
  const first = transaction.discard();
  const second = transaction.discard();
  assert.equal(first, second);
  assert.throws(() => transaction.commit(), /no longer active/u);
  await Promise.resolve();
  assert.equal(calls, 1);
  finish();
  await Promise.all([first, second]);
  await transaction.discard();
  assert.equal(calls, 1);
  assert.throws(() => transaction.commit(), /no longer active/u);
});

test("concurrent cleanup observes the same rejection and can retry", { timeout: 3000 }, async () => {
  let fail;
  let calls = 0;
  const parent = { removeEntry() {
    calls++;
    return calls === 1 ? new Promise((resolve, reject) => { fail = reject; }) : Promise.resolve();
  } };
  const transaction = new OutputDirectoryTransaction(parent, "partial", {});
  const first = transaction.discard();
  const second = transaction.discard();
  const results = Promise.allSettled([first, second]);
  await Promise.resolve();
  const error = new Error("busy");
  fail(error);
  assert.deepEqual(await results, [{ status: "rejected", reason: error }, { status: "rejected", reason: error }]);
  await transaction.discard();
  assert.equal(calls, 2);
});

test("synchronous removal failure restores an active transaction", async () => {
  const transaction = new OutputDirectoryTransaction({ removeEntry() { throw new Error("busy"); } }, "partial", {});
  await assert.rejects(transaction.discard(), /busy/u);
  transaction.commit();
  await transaction.discard();
});
