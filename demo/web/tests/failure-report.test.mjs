import assert from "node:assert/strict";
import { test } from "node:test";
import { failureReport } from "../failure-report.mjs";

test("names the input, the phase and the libraries with the failure", () => {
  const report = failureReport({
    message: "FAIL: Executable output would overwrite the input executable",
    phase: "Converting PRX modules",
    input: "PPSA00000.zip \u00b7 app0/sce_module/libScePad.prx",
    target: "windows",
    release: { tag: "v1.4.4" },
    log: ["Inspecting archive", "FAIL: Executable output would overwrite the input executable"]
  });
  assert.match(report, /^FAIL: Executable output would overwrite the input executable/u);
  assert.match(report, /Phase: Converting PRX modules/u);
  assert.match(report, /Target: windows/u);
  assert.match(report, /Libraries: v1\.4\.4/u);
  assert.match(report, /Conversion log \(last 2 lines\):\nInspecting archive\nFAIL:/u);
});

test("keeps only the last 150 log lines", () => {
  const log = Array.from({ length: 200 }, (_, index) => `line ${index + 1}`);
  const report = failureReport({ message: "cancelled", log });
  const lines = report.split("\n").filter(line => line.startsWith("line "));
  assert.equal(lines.length, 150);
  assert.equal(lines[0], "line 51");
  assert.equal(lines.at(-1), "line 200");
});

test("prints a bare message when there is no context or log", () => {
  assert.equal(failureReport({ message: "cancelled" }), "cancelled");
  assert.equal(failureReport({ message: "cancelled", log: [] }), "cancelled");
});
