import assert from "node:assert/strict";
import { test } from "node:test";
import { batchMemoryEstimate, batchOutputDirectory, memoryEstimate, MEMORY_LIMIT } from "../batch.mjs";

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
