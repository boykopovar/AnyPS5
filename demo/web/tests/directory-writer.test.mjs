import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { fileURLToPath } from "node:url";
import { test } from "node:test";

test("directory writer prepares its reader and cleans acquired stream resources", async () => {
  const { stdout } = await promisify(execFile)(process.execPath, ["--experimental-vm-modules", fileURLToPath(new URL("./directory-writer-fixture.mjs", import.meta.url))], { timeout: 15000 });
  assert.ok(JSON.parse(stdout).scenarios >= 6);
});
