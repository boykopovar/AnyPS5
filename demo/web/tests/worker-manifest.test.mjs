import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { fileURLToPath } from "node:url";
import { test } from "node:test";

test("library manifest validation runs before downloading and preserves valid downloads", async () => {
  const { stdout } = await promisify(execFile)(process.execPath, ["--experimental-vm-modules", fileURLToPath(new URL("./worker-manifest-fixture.mjs", import.meta.url))], { timeout: 15000 });
  const result = JSON.parse(stdout);
  assert.ok(result.invalid >= 15);
  assert.equal(result.valid, 4);
});
