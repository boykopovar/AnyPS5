import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";
import { webcrypto } from "node:crypto";

const bytes = new Uint8Array([1, 2, 3]);
const digest = Buffer.from(await webcrypto.subtle.digest("SHA-256", bytes)).toString("hex");
const valid = target => ({ tag: "v1.2.3", platforms: { [target]: { size: 3, expandedBytes: 100, sha256: digest, path: `libraries/${target}-${digest}.zip` } } });

async function load(release, options = {}) {
  const requests = [];
  let cancelled = false;
  const context = vm.createContext({ console, Uint8Array, Blob, ReadableStream, WritableStream, performance, crypto: webcrypto,
    self: { postMessage() {} }, fetch: async url => {
      requests.push(url);
      if (url === "./release.json") return { ok: true, json: async () => release };
      if (url.startsWith("https:")) return options.rateLimit ? { ok: false, status: 403, headers: new Map([["x-ratelimit-remaining", "0"]]) } : { ok: true, json: async () => ({ tag_name: release.tag }) };
      const contents = options.contents ?? bytes;
      return { ok: true, body: new ReadableStream({ start(controller) { controller.enqueue(contents); if (!options.keepOpen) controller.close(); }, cancel() { cancelled = true; } }) };
    } });
  const module = new vm.SourceTextModule(readFileSync(new URL("../worker.mjs", import.meta.url), "utf8"), { context });
  await module.link(async name => {
    const exports = name === "./batch.mjs" ? { batchMemoryEstimate() {}, batchOutputDirectory() {}, MEMORY_LIMIT: 0 } : { Archive: class {}, CONVERSION_LIMIT: 0, createPackageWriter() {}, packageArchive() {} };
    return new vm.SyntheticModule(Object.keys(exports), function() { for (const [key, value] of Object.entries(exports)) this.setExport(key, value); }, { context });
  });
  await module.evaluate();
  return { api: module.namespace, requests, cancelled: () => cancelled };
}

const invalid = [null, [], {}, { tag: "v1", platforms: null }, { ...valid("linux"), tag: "" }, { ...valid("linux"), tag: "v1." }];
for (const [key, value] of [["path", `libraries/windows-${digest}.zip`], ["path", "libraries/linux-a.zip"], ["sha256", null], ["sha256", "a".repeat(63)], ["sha256", 123], ["size", 0], ["size", -1], ["size", 1.5], ["size", "3"], ["size", 512 * 1024 * 1024 + 1], ["expandedBytes", -1], ["expandedBytes", 1.5], ["expandedBytes", null], ["expandedBytes", 512 * 1024 * 1024 + 1]]) {
  const release = valid("linux");
  release.platforms.linux[key] = value;
  invalid.push(release);
}
for (const release of invalid) {
  const { api, requests } = await load(release);
  await assert.rejects(api.downloadLibraries("linux"), /Invalid release manifest/);
  assert.deepEqual(requests, ["./release.json"]);
}
const unsupported = await load(valid("linux"));
await assert.rejects(unsupported.api.downloadLibraries("macos"), /Invalid library target/);
assert.deepEqual(unsupported.requests, []);
let validCount = 0;
for (const target of ["linux", "windows"]) {
  for (const rateLimit of [false, true]) {
    const release = valid(target);
    const { api, requests } = await load(release, { rateLimit });
    const downloaded = await api.downloadLibraries(target);
    assert.equal(downloaded.expandedBytes, 100);
    assert.deepEqual(new Uint8Array(await downloaded.file.arrayBuffer()), bytes);
    assert.equal(requests.at(-1), release.platforms[target].path);
    validCount++;
  }
}
const corrupt = await load(valid("linux"), { contents: new Uint8Array([3, 2, 1]) });
await assert.rejects(corrupt.api.downloadLibraries("linux"), /checksum mismatch/);
const oversized = await load(valid("linux"), { contents: new Uint8Array([1, 2, 3, 4]), keepOpen: true });
await assert.rejects(oversized.api.downloadLibraries("linux"), /download is incomplete/);
assert.equal(oversized.cancelled(), true);
console.log(JSON.stringify({ invalid: invalid.length + 1, valid: validCount }));
