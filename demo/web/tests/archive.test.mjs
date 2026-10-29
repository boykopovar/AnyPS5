import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { pathToFileURL } from "node:url";
import path from "node:path";
import { register } from "node:module";

register("./cdn-loader.mjs", import.meta.url, { data: { url: process.env.ANYPS5_TEST_ZIP_URL, path: process.env.ANYPS5_TEST_ZIP_PATH } });

const site = name => pathToFileURL(path.join(process.argv[2], name)).href;
const { Archive, memoryEstimate, MEMORY_LIMIT, ICON_LIMIT, packageArchive } = await import(site("archive.mjs"));
const { BlobReader, BlobWriter, Uint8ArrayReader, Uint8ArrayWriter, ZipReader, ZipWriter } = await import(process.env.ANYPS5_TEST_ZIP_URL);
await import("./archive-reserved.test.mjs");
const { default: createRelinker } = await import(site("native/relinker_wasm.mjs"));
const native = await createRelinker();
const executable = new Uint8Array(await readFile(path.join(process.argv[3], "main.elf")));
const module = new Uint8Array(await readFile(path.join(process.argv[3], "module.elf")));
const resource = new Uint8Array(2 * 1024 * 1024).fill(65);

async function zip(entries) {
  const output = new BlobWriter("application/zip");
  const writer = new ZipWriter(output, { zip64: true, level: 9, useWebWorkers: false, useCompressionStream: false });
  for (const [name, bytes, options] of entries) await writer.add(name, bytes ? new Uint8ArrayReader(bytes) : undefined, options);
  return writer.close();
}

const source = await zip([["title/", null, { directory: true }], ["title/program.dat", executable], ["title/sce_module/library.prx.esbak", module], ["title/sce_module/library.prx", new Uint8Array([79, 21, 61, 29])], ["title/sce_module/library.prx.esbak.guest.prx", new Uint8Array([77, 90, 0, 0])], ["title/sce_sys/param.json", new TextEncoder().encode("{}")], ["title/shaders/example.frag", executable], ["title/content/resource.bin", resource]]);
const folder = [
  { path: "title/program.dat", file: new Blob([executable]) },
  { path: "title/sce_module/library.prx.esbak", file: new Blob([module]) },
  { path: "title/sce_module/library.prx", file: new Blob([new Uint8Array([79, 21, 61, 29])]) },
  { path: "title/sce_module/library.prx.esbak.guest.prx", file: new Blob([new Uint8Array([77, 90, 0, 0])]) },
  { path: "title/sce_sys/param.json", file: new Blob(["{}"]) },
  { path: "title/shaders/example.frag", file: new Blob([executable]) },
  { path: "title/content/resource.bin", file: new Blob([resource]) }
];
for (const [kind, selected] of [["ZIP", source], ["folder", folder]]) {
for (const target of ["windows", "linux"]) {
  const archive = new Archive(selected);
  const summary = await archive.inspect(() => {});
  assert.equal(summary.name, "program.dat");
  assert.equal(summary.root, "title/");
  assert.equal(summary.modules, 1);
  assert.equal(summary.selectedBytes, executable.length + module.length);
  const input = await archive.conversionInput(target, false, () => {});
  assert.equal(input.modules.length, 1);
  assert.deepEqual(input.executable, executable);
  assert.equal(input.modules[0].name, "sce_module/library.prx.esbak");
  const events = [];
  const converted = native.Convert(input, event => events.push(event));
  assert.equal(converted.ok, true, converted.error);
  assert.equal(converted.executable[0], target === "windows" ? 77 : 127);
  if (target === "windows") {
    const image = new DataView(converted.executable.buffer, converted.executable.byteOffset, converted.executable.byteLength);
    assert.equal(image.getUint16(image.getUint32(60, true) + 24 + 68, true), 3);
  }
  assert(events.some(event => event.stage === 6 && event.completed > 0));
  const libraries = await zip([["libs/system.prx", new Uint8Array([1, 2, 3])]]);
  const chunks = [];
  let maxChunk = 0;
  const destination = new WritableStream({ async write(chunk) {
    maxChunk = Math.max(maxChunk, chunk.length);
    chunks.push(chunk.slice());
    await new Promise(resolve => setTimeout(resolve, 1));
  } });
  await packageArchive(archive, converted, input.outputName, libraries, destination, () => {});
  await destination.close();
  const reader = new ZipReader(new BlobReader(new Blob(chunks)));
  const entries = await reader.getEntries();
  const byName = new Map(entries.map(entry => [entry.filename, entry]));
  assert(byName.has(input.outputName));
  assert(byName.has("libs/system.prx"));
  assert.deepEqual(await byName.get("app0/shaders/example.frag").getData(new Uint8ArrayWriter(), { checkSignature: true }), executable);
  assert.deepEqual(await byName.get("app0/sce_module/library.prx.esbak.guest.prx").getData(new Uint8ArrayWriter(), { checkSignature: true }), converted.modules[0].bytes);
  assert.deepEqual(await byName.get("app0/sce_module/library.prx").getData(new Uint8ArrayWriter(), { checkSignature: true }), new Uint8Array([79, 21, 61, 29]));
  assert.deepEqual(await byName.get("app0/program.dat").getData(new Uint8ArrayWriter(), { checkSignature: true }), executable);
  assert.deepEqual(await byName.get("app0/content/resource.bin").getData(new Uint8ArrayWriter(), { checkSignature: true }), resource);
  assert.equal((byName.get(input.outputName).externalFileAttributes >>> 16) & 0o777, 0o755);
  assert(maxChunk < resource.length);
  const saved = new Map();
  let active = false;
  let directoryChunk = 0;
  await packageArchive(archive, converted, input.outputName, libraries, {
    async directory(name) { saved.set(name, null); },
    async file(name) {
      assert.equal(active, false);
      active = true;
      const pieces = [];
      return new WritableStream({
        async write(chunk) {
          directoryChunk = Math.max(directoryChunk, chunk.length);
          pieces.push(chunk.slice());
          await new Promise(resolve => setTimeout(resolve, 1));
        },
        async close() {
          saved.set(name, new Uint8Array(await new Blob(pieces).arrayBuffer()));
          active = false;
        }
      });
    }
  }, () => {}, true);
  assert.equal(active, false);
  assert(directoryChunk < resource.length);
  assert.equal(saved.size, entries.length);
  for (const entry of entries) {
    assert(saved.has(entry.filename));
    if (!entry.directory) assert.deepEqual(saved.get(entry.filename), await entry.getData(new Uint8ArrayWriter(), { checkSignature: true }));
  }
  await reader.close();
  await archive.close();
  console.log(`${kind}, ${target}: conversion, original files, modules, permissions, streamed ZIP and directory output verified`);
}
}

const bundleArchive = new Archive(source);
await bundleArchive.inspect(() => {});
const bundleInput = await bundleArchive.conversionInput("linux", false, () => {});
const bundleConversion = native.Convert(bundleInput, () => {});
assert.equal(bundleConversion.ok, true, bundleConversion.error);
const bundleLibraries = await zip([["libs/system.prx", new Uint8Array([1, 2, 3])]]);
const bundleWriter = new ZipWriter(new BlobWriter("application/zip"), { zip64: true, level: 9, useWebWorkers: false, useCompressionStream: false });
await packageArchive(bundleArchive, bundleConversion, bundleInput.outputName, bundleLibraries, null, () => {}, false, { prefix: "01-first/", writer: bundleWriter });
await packageArchive(bundleArchive, bundleConversion, bundleInput.outputName, bundleLibraries, null, () => {}, false, { prefix: "02-second/", writer: bundleWriter });
const bundle = await bundleWriter.close();
const bundleReader = new ZipReader(new BlobReader(bundle));
const bundleEntries = await bundleReader.getEntries();
const bundleNames = new Set(bundleEntries.map(entry => entry.filename));
assert(bundleNames.has(`01-first/${bundleInput.outputName}`));
assert(bundleNames.has("01-first/libs/system.prx"));
assert(bundleNames.has("01-first/app0/content/resource.bin"));
assert(bundleNames.has(`02-second/${bundleInput.outputName}`));
assert(bundleNames.has("02-second/libs/system.prx"));
assert(bundleNames.has("02-second/app0/content/resource.bin"));
await bundleReader.close();
await bundleArchive.close();
console.log("Multiple app packages append under separate roots in a single ZIP");

for (const [entries, error] of [
  [[["one", executable], ["two", executable]], /exactly one/u],
  [[["readme", new Uint8Array([1])]], /exactly one/u],
  [[["eboot.bin", new Uint8Array([79, 21, 61, 29])]], /SELF container/u],
  [[["eboot.bin", new Uint8Array([84, 20, 245, 238])]], /SELF container/u],
  [[["app", executable], ["..\/escape", resource]], /Unsafe ZIP/u],
  [[["app", executable], ["APP", resource]], /Duplicate ZIP/u],
  [[["app", executable], ["folder", resource], ["folder/file", resource]], /also used as a folder/u],
  [[["app", executable], ["link", resource, { versionMadeBy: 0x31e, externalFileAttributes: (0o120777 << 16) >>> 0 }]], /Links and special/u]
]) {
  const archive = new Archive(await zip(entries));
  await assert.rejects(archive.inspect(() => {}), error);
  await archive.close();
}
const missing = new Archive(await zip([["any-name", executable]]));
assert.equal((await missing.inspect(() => {})).warnings.length, 2);
await missing.close();
const selfModule = new Archive(await zip([["program.dat", executable],["sce_module/library.prx", new Uint8Array([84, 20, 245, 238])]]));
const selfModuleSummary = await selfModule.inspect(() => {});
assert.equal(selfModuleSummary.warnings.length, 2);
assert.match(selfModuleSummary.warnings.join(" "), /SELF container/u);
await selfModule.close();
assert(memoryEstimate(200 * 1024 ** 3, 1024, 1024, 100) > MEMORY_LIMIT);
const invalid = native.Convert({ target: "unsupported" }, () => {});
assert.equal(invalid.ok, false);
console.log("Archive validation, missing-folder warnings and large-download budget verified");

const iconBytes = new Uint8Array(1024 * 1024);
const iconArchive = new Archive([
  { path: "title/program.elf", file: new Blob([new Uint8Array([0x7f, 69, 76, 70])]) },
  { path: "title/sce_sys/icon0.png", file: new Blob([iconBytes]) }
]);
const iconSummary = await iconArchive.inspect(() => {});
assert.equal(iconSummary.selectedBytes, 4);
assert.equal(iconSummary.windowsSelectedBytes, iconBytes.length + 4);
assert.equal((await iconArchive.conversionInput("linux", false, () => {})).icon, null);
assert.equal((await iconArchive.conversionInput("windows", false, () => {})).icon.length, iconBytes.length);
const librariesAtBudgetEdge = MEMORY_LIMIT
  - memoryEstimate(iconSummary.totalBytes, iconSummary.selectedBytes, 0, iconSummary.files)
  - iconBytes.length * 2;
assert(memoryEstimate(iconSummary.totalBytes, iconSummary.selectedBytes, librariesAtBudgetEdge, iconSummary.files) < MEMORY_LIMIT);
assert(memoryEstimate(iconSummary.totalBytes, iconSummary.windowsSelectedBytes, librariesAtBudgetEdge, iconSummary.files) > MEMORY_LIMIT);
await iconArchive.close();
console.log("Linux conversion input and memory estimate exclude the Windows-only icon");

const oversizedIconArchive = new Archive([
  { path: "title/program.elf", file: new Blob([new Uint8Array([0x7f, 69, 76, 70])]) },
  { path: "title/sce_sys/icon0.png", file: new Blob([new Uint8Array(ICON_LIMIT + 1)]) }
]);
const oversizedIconSummary = await oversizedIconArchive.inspect(() => {});
assert.equal(oversizedIconSummary.windowsIconTooLarge, true);
assert.equal(oversizedIconArchive.selectedBytesForTarget("linux"), 4);
assert.throws(() => oversizedIconArchive.selectedBytesForTarget("windows"), /16 MiB/u);
assert.equal((await oversizedIconArchive.conversionInput("linux", false, () => {})).icon, null);
await oversizedIconArchive.close();
console.log("Oversized Windows-only icons do not block Linux conversion");

const workerMessages = [];
globalThis.self = { postMessage: message => workerMessages.push(message) };
const worker = await import(site("worker.mjs"));
const libraryArchive = await zip([["libs/system.prx", new Uint8Array([1, 2, 3])]]);
const libraryBytes = new Uint8Array(await libraryArchive.arrayBuffer());
const libraryPath = "libraries/windows-acde1234.zip";
const librarySha256 = [...new Uint8Array(await crypto.subtle.digest("SHA-256", libraryBytes))]
  .map(byte => byte.toString(16).padStart(2, "0"))
  .join("");
const release = {
  tag: "v1",
  platforms: {
    windows: {
      path: libraryPath,
      size: libraryBytes.byteLength,
      expandedBytes: 3,
      sha256: librarySha256
    }
  }
};
const originalFetch = globalThis.fetch;
let latestResponse = new Response(null, { status: 403, headers: { "X-RateLimit-Remaining": "0" } });
globalThis.fetch = async url => {
  if (url === "./release.json") return new Response(JSON.stringify(release));
  if (url === "https://api.github.com/repos/boykopovar/AnyPS5/releases/latest") return latestResponse;
  if (url === libraryPath) return new Response(libraryBytes);
  throw new Error(`Unexpected fetch: ${url}`);
};
try {
  const downloaded = await worker.downloadLibraries("windows");
  assert.equal(downloaded.release.tag, release.tag);
  assert.deepEqual(new Uint8Array(await downloaded.file.arrayBuffer()), libraryBytes);
  assert(workerMessages.some(message => message.type === "progress" && /rate limit reached/u.test(message.message ?? "")));

  latestResponse = new Response(null, { status: 429 });
  const rateLimitedDownload = await worker.downloadLibraries("windows");
  assert.deepEqual(new Uint8Array(await rateLimitedDownload.file.arrayBuffer()), libraryBytes);

  latestResponse = new Response(null, { status: 403 });
  await assert.rejects(worker.downloadLibraries("windows"), /Cannot verify the latest release \(403\)/u);
  latestResponse = new Response(JSON.stringify({ tag_name: "v2" }));
  await assert.rejects(worker.downloadLibraries("windows"), /A new release is being published/u);

  latestResponse = new Response(null, { status: 429 });
  await self.onmessage({ data: { type: "inspect", inputs: [
    { name: "First App.zip", file: source },
    { name: "First App.zip", file: source }
  ] } });
  assert.equal(workerMessages.filter(message => message.type === "inspected").length, 2);
  assert(workerMessages.some(message => message.type === "inspection-complete" && message.count === 2));
  await self.onmessage({ data: { type: "convert", target: "windows", toIntel: false, mode: "memory" } });
  const batchComplete = workerMessages.find(message => message.type === "complete");
  assert(batchComplete, workerMessages.find(message => message.type === "error")?.message);
  assert.equal(batchComplete.apps, 2);
  const batchReader = new ZipReader(new BlobReader(batchComplete.blob));
  const batchNames = new Set((await batchReader.getEntries()).map(entry => entry.filename));
  assert(batchNames.has("01-First-App/program.exe"));
  assert(batchNames.has("01-First-App/libs/system.prx"));
  assert(batchNames.has("01-First-App/app0/content/resource.bin"));
  assert(batchNames.has("02-First-App/program.exe"));
  assert(batchNames.has("02-First-App/libs/system.prx"));
  assert(batchNames.has("02-First-App/app0/content/resource.bin"));
  await batchReader.close();
  workerMessages.length = 0;
} finally {
  globalThis.fetch = originalFetch;
  workerMessages.length = 0;
}
await self.onmessage({ data: { type: "convert", target: "windows", mode: "disk" } });
assert.equal(workerMessages.length, 1);
assert.equal(workerMessages[0].type, "error");
assert.match(workerMessages[0].message, /browsers block DLL creation/u);
workerMessages.length = 0;
await self.onmessage({ data: { type: "inspect", inputs: [
  { name: "valid.zip", file: source },
  { name: "invalid.zip", file: new Blob(["not a ZIP"]) }
] } });
assert.equal(workerMessages.at(-1).type, "error");
assert.match(workerMessages.at(-1).message, /invalid\.zip/u);
assert.equal(workerMessages.some(message => message.type === "inspection-complete"), false);
delete globalThis.self;
console.log("Rate-limited release checks use the verified site manifest; other release-check failures remain errors");
console.log("Windows folder output rejected before conversion or file writes");
console.log("Multiple ZIP inputs stream into separate app roots; invalid inputs name the failing archive");

for (const compression of [12, 14]) {
  const stored = await zip([["libs/system.prx", new Uint8Array([1, 2, 3]), { level: 0 }]]);
  const bytes = new Uint8Array(await stored.arrayBuffer());
  const view = new DataView(bytes.buffer);
  for (let offset = 0; offset + 46 <= bytes.length; offset++) {
    const signature = view.getUint32(offset, true);
    if (signature === 0x04034b50) view.setUint16(offset + 8, compression, true);
    if (signature === 0x02014b50) view.setUint16(offset + 10, compression, true);
  }
  let writes = 0;
  await assert.rejects(packageArchive({ files: [] }, { executable: new Uint8Array([1]), modules: [] }, "program.elf",
    new Blob([bytes]), { async file() { writes++; throw new Error("destination opened"); } }, () => {}, true), /Unsupported ZIP compression/u);
  assert.equal(writes, 0);
}
for (const mode of [0o010644, 0o020644, 0o060644, 0o140644]) {
  const special = await zip([["libs/special", new Uint8Array([1]), { versionMadeBy: 0x31e, externalFileAttributes: (mode << 16) >>> 0 }]]);
  let writes = 0;
  await assert.rejects(packageArchive({ files: [] }, { executable: new Uint8Array([1]), modules: [] }, "program.elf",
    special, { async file() { writes++; throw new Error("destination opened"); } }, () => {}, true), /special files/u);
  assert.equal(writes, 0);
}
