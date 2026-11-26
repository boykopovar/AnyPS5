import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";
import { webcrypto } from "node:crypto";
import { translate } from "../i18n.mjs";

const bytes = new Uint8Array([1, 2, 3]);
const digest = Buffer.from(await webcrypto.subtle.digest("SHA-256", bytes)).toString("hex");
const valid = target => ({ tag: "v1.2.3", platforms: { [target]: { size: 3, expandedBytes: 100, sha256: digest, path: `libraries/${target}-${digest}.zip` } } });

async function load(release, options = {}) {
  const requests = [];
  const progress = [];
  let cancelled = false;
  const context = vm.createContext({ console, Uint8Array, Blob, ReadableStream, WritableStream, performance, crypto: webcrypto,
    self: { postMessage(event) { progress.push(event); } }, fetch: async url => {
      requests.push(url);
      if (url === "./release.json") return { ok: true, json: async () => release };
      if (url.startsWith("https:")) return options.latestResponse ?? (options.rateLimit ? { ok: false, status: 403, headers: new Map([["x-ratelimit-remaining", "0"]]) } : { ok: true, json: async () => ({ tag_name: release.tag }) });
      if (options.noBody) return new Response(null, { status: 204 });
      const contents = options.contents ?? bytes;
      return { ok: true, body: new ReadableStream({ start(controller) { controller.enqueue(contents); if (!options.keepOpen) controller.close(); }, cancel() { cancelled = true; } }) };
    } });
  const module = new vm.SourceTextModule(readFileSync(new URL("../worker.mjs", import.meta.url), "utf8"), {
    context,
    async importModuleDynamically() {
      if (!options.native) throw new Error("fixture-stop");
      const native = new vm.SyntheticModule(["default"], function() {
        this.setExport("default", () => options.native);
      }, { context });
      await native.link(() => {});
      await native.evaluate();
      return native;
    }
  });
  await module.link(async name => {
    const exports = name === "./batch.mjs" ? { batchMemoryEstimate() {}, batchOutputDirectory() {}, MEMORY_LIMIT: 0 }
      : name === "./i18n.mjs" ? { translate }
      : { Archive: options.Archive ?? class {}, CONVERSION_LIMIT: 0, createPackageWriter() {}, packageArchive: options.packageArchive ?? (() => {}) };
    return new vm.SyntheticModule(Object.keys(exports), function() { for (const [key, value] of Object.entries(exports)) this.setExport(key, value); }, { context });
  });
  await module.evaluate();
  return { api: module.namespace, self: context.self, requests, progress, cancelled: () => cancelled };
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
const workerProgress = await load(valid("linux"), { Archive: class {
  async inspect(report) {
    report({ phase: translate("en", "inspectingFiles"), phaseKey: "inspectingFiles", input: "main.elf", completed: 1, total: 2 });
    return {};
  }
  async close() {}
} });
await workerProgress.self.onmessage({ data: { type: "inspect", locale: "en", inputs: [{ name: "game.zip" }] } });
const event = workerProgress.progress.find(item => item.type === "progress");
assert.equal(event.phaseKey, "inspectingFiles");
assert.equal(event.phase, translate("en", "inspectingFiles"));
assert.equal(event.appName, "game.zip");
assert.equal(event.completed, 1);
await workerProgress.self.onmessage({ data: { type: "convert", locale: "en", target: "linux", mode: "memory" } });
assert(workerProgress.progress.some(item => item.type === "progress" && item.phaseKey === "phaseLoadingConverter"));
const revisionSha = "0123456789abcdef0123456789abcdef01234567";
for (const [revision, expectedRevision, target, toIntel] of [
  [revisionSha, revisionSha, "linux", true],
  [revisionSha, revisionSha, "windows", false],
  [undefined, null, "linux", false],
  [revisionSha + "\n", null, "linux", false],
  ["wrong", null, "linux", false]
]) {
  const release = { ...valid(target), revision };
  const archiveOptions = [];
  const conversion = await load(release, {
    Archive: class {
      async inspect() { return { selectedBytes: 0 }; }
      async conversionInput() { return { outputName: "app.elf" }; }
      async close() {}
    },
    native: { Convert() { return { ok: true }; } },
    async packageArchive(_archive, _converted, _name, _libraries, _destination, _report, _directory, options) {
      archiveOptions.push(options);
    }
  });
  await conversion.self.onmessage({ data: { type: "inspect", inputs: [{ name: "private source.zip" }] } });
  await conversion.self.onmessage({ data: { type: "convert", target, toIntel, mode: "memory" } });
  const complete = conversion.progress.find(item => item.type === "complete");
  assert.ok(complete, JSON.stringify(conversion.progress));
  assert.equal(complete.release, release.tag);
  assert.equal(archiveOptions.length, 1);
  assert.equal(complete.revision, expectedRevision);
  assert.deepEqual(JSON.parse(JSON.stringify(archiveOptions[0].receipt)), {
    version: 1, release: { tag: release.tag, revision: expectedRevision }, target, toIntel
  });
}
let validCount = 0;
for (const target of ["linux", "windows"]) {
  for (const rateLimit of [false, true]) {
    const release = valid(target);
    const { api, requests, progress } = await load(release, { rateLimit });
    const downloaded = await api.downloadLibraries(target);
    assert.equal(downloaded.expandedBytes, 100);
    assert.deepEqual(new Uint8Array(await downloaded.file.arrayBuffer()), bytes);
    assert.equal(requests.at(-1), release.platforms[target].path);
    assert(progress.some(event => event.phaseKey === "phaseDownloadingLibraries"));
    validCount++;
  }
}
const throttleResponse = new Response(null, {
  status: 403, headers: { "x-ratelimit-remaining": "57", "retry-after": "60" }
});
const secondary = await load(valid("linux"), { latestResponse: throttleResponse });
const published = await secondary.api.downloadLibraries("linux");
assert.deepEqual(new Uint8Array(await published.file.arrayBuffer()), bytes);
assert.deepEqual(secondary.requests, [
  "./release.json", "https://api.github.com/repos/boykopovar/AnyPS5/releases/latest", valid("linux").platforms.linux.path
]);
const corruptDuringThrottle = await load(valid("linux"), { latestResponse: throttleResponse, contents: new Uint8Array([3, 2, 1]) });
await assert.rejects(corruptDuringThrottle.api.downloadLibraries("linux"), /checksum mismatch/);
const denied = await load(valid("linux"), { latestResponse: new Response(null, {
  status: 403, headers: { "x-ratelimit-remaining": "57" }
}) });
await assert.rejects(denied.api.downloadLibraries("linux"), /Cannot verify the latest release \(403\)/);
assert.deepEqual(denied.requests, ["./release.json", "https://api.github.com/repos/boykopovar/AnyPS5/releases/latest"]);
const corrupt = await load(valid("linux"), { contents: new Uint8Array([3, 2, 1]) });
await assert.rejects(corrupt.api.downloadLibraries("linux"), /checksum mismatch/);
const oversized = await load(valid("linux"), { contents: new Uint8Array([1, 2, 3, 4]), keepOpen: true });
await assert.rejects(oversized.api.downloadLibraries("linux"), /download is incomplete/);
assert.equal(oversized.cancelled(), true);
const noBody = await load(valid("linux"), { noBody: true });
await assert.rejects(noBody.api.downloadLibraries("linux"), /download is incomplete/);
console.log(JSON.stringify({ invalid: invalid.length + 1, valid: validCount }));
