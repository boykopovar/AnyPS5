import assert from "node:assert/strict";
import { test } from "node:test";
import { batchMemoryEstimate, batchOutputDirectory, memoryEstimate, MEMORY_LIMIT } from "../batch.mjs";

test("rejects each invalid memory operand before arithmetic can coerce or cancel it", () => {
  for (let index = 0; index < 4; index++) {
    for (const value of [-1, -100000000, "1024", null, true, NaN, Infinity, 1.5, Number.MAX_SAFE_INTEGER + 1]) {
      const operands = [1024, 70 * 1024 * 1024, 0, 2];
      operands[index] = value;
      assert.throws(() => memoryEstimate(...operands), /unsupported file size/u);
    }
  }
  const summaries = [{totalBytes: 1024, selectedBytes: 70 * 1024 * 1024, windowsSelectedBytes: 70 * 1024 * 1024, files: 2}];
  assert.ok(batchMemoryEstimate(summaries, "linux", 0) > MEMORY_LIMIT);
  assert.throws(() => batchMemoryEstimate(summaries, "linux", -100000000), /unsupported file size/u);
  assert.equal(memoryEstimate(0, 0, 0, 0), 8 * 1024 * 1024);
});

test("estimates all ZIP outputs against one shared memory limit", () => {
  const summaries = [
    { totalBytes: 1024, selectedBytes: 128, windowsSelectedBytes: 256, files: 2 },
    { totalBytes: 2048, selectedBytes: 256, windowsSelectedBytes: 512, files: 3 }
  ];

  assert.equal(batchMemoryEstimate(summaries, "linux", 1024), summaries.reduce(
    (total, summary) => total + memoryEstimate(summary.totalBytes, summary.selectedBytes, 1024, summary.files),
    0
  ));
  assert.equal(batchMemoryEstimate(summaries, "windows", 1024), summaries.reduce(
    (total, summary) => total + memoryEstimate(summary.totalBytes, summary.windowsSelectedBytes, 1024, summary.files),
    0
  ));
  assert.throws(() => batchMemoryEstimate([], "macos", 0), /Unsupported conversion target/u);
  assert.throws(() => batchMemoryEstimate([{ totalBytes: Number.MAX_SAFE_INTEGER, selectedBytes: 0, windowsSelectedBytes: 0, files: 0 }], "linux", 0), /unsupported file size/u);
  assert.equal(MEMORY_LIMIT, 256 * 1024 * 1024);
});

test("creates safe and distinct ZIP folder names for duplicate app filenames", () => {
  assert.equal(batchOutputDirectory("First App.zip", 0), "01-First-App");
  assert.equal(batchOutputDirectory("First App.zip", 1), "02-First-App");
  assert.equal(batchOutputDirectory("CON.zip", 0), "01-CON");
  assert.equal(batchOutputDirectory("...zip", 0), "01-app");
  assert.throws(() => batchOutputDirectory("app.zip", -1), /Invalid app index/u);
});
