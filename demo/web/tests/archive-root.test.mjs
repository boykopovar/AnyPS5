import assert from "node:assert/strict";
import { register } from "node:module";
import { pathToFileURL } from "node:url";
import path from "node:path";

register("./cdn-loader.mjs", import.meta.url, { data: { url: process.env.ANYPS5_TEST_ZIP_URL, path: process.env.ANYPS5_TEST_ZIP_PATH } });
const { Archive, DirectoryWriter } = await import(pathToFileURL(path.join(process.argv[2], "archive.mjs")));
const { BlobWriter, Uint8ArrayReader, ZipWriter } = await import(process.env.ANYPS5_TEST_ZIP_URL);
const header = new Uint8Array([127, 69, 76, 70]);

async function zipped(entries) {
  const writer = new ZipWriter(new BlobWriter(), { useWebWorkers: false, useCompressionStream: false });
  for (const [name, directory, bytes = header, options = {}] of entries) await writer.add(name, directory ? undefined : new Uint8ArrayReader(bytes), { directory, ...options });
  return writer.close();
}

const metadata = new Uint8Array(86);
const metadataView = new DataView(metadata.buffer);
metadataView.setUint32(0, 0x00051607);
metadataView.setUint32(4, 0x00020000);
metadataView.setUint16(24, 2);
metadataView.setUint32(26, 9);
metadataView.setUint32(30, 50);
metadataView.setUint32(34, 32);
metadataView.setUint32(38, 2);
metadataView.setUint32(42, 82);
metadataView.setUint32(46, 4);
const app = [["Title/main.elf", false], ["Title/sce_module/provider.prx", false]];
const sidecars = [["__MACOSX/Title/._main.elf", false, metadata], ["__MACOSX/Title/sce_module/._provider.prx", false, metadata]];
for (const directories of [[], ["Title/", "Title/sce_module/", "__MACOSX/", "__MACOSX/Title/", "__MACOSX/Title/sce_module/"]]) {
  const archive = new Archive(await zipped([...directories.map(name => [name, true]), ...app, ...sidecars, ["__MACOSX/._Title", false, metadata]]));
  try {
    const summary = await archive.inspect(() => {});
    assert.equal(summary.root, "Title/");
    assert.equal(summary.files, 2);
    assert.equal(summary.modules, 1);
    assert.equal(summary.selectedBytes, 8);
    assert.equal(summary.totalBytes, 8 + metadata.length * 3);
    assert(!archive.files.some(item => item.name.includes("__MACOSX")));
    for (const target of ["linux", "windows"]) {
      const input = await archive.conversionInput(target, false, () => {});
      assert.deepEqual(input.executable, header);
      assert.equal(input.modules[0].name, "sce_module/provider.prx");
      assert.deepEqual(input.modules[0].bytes, header);
    }
  } finally { await archive.close(); }
}

const wrongMagic = metadata.slice();
wrongMagic[3] = 0;
const wrongVersion = metadata.slice();
wrongVersion[5] = 1;
const oversizedTable = metadata.slice();
new DataView(oversizedTable.buffer).setUint16(24, 100);
const invalidDescriptors = [];
for (const [position, value] of [
  [26, 0], [26, 1], [38, 9], [30, 0], [30, 49],
  [30, 0xffffffff], [34, 0xffffffff], [42, 83], [42, 87]
]) {
  const invalid = metadata.slice();
  new DataView(invalid.buffer).setUint32(position, value);
  invalidDescriptors.push(invalid);
}
const emptyBeforeTable = metadata.slice();
new DataView(emptyBeforeTable.buffer).setUint32(30, 25);
new DataView(emptyBeforeTable.buffer).setUint32(34, 0);
invalidDescriptors.push(emptyBeforeTable);
const emptyPastEnd = metadata.slice();
new DataView(emptyPastEnd.buffer).setUint32(42, 87);
new DataView(emptyPastEnd.buffer).setUint32(46, 0);
invalidDescriptors.push(emptyPastEnd);
for (const extra of [
  [["__MACOSX/Title/._main.elf", false, wrongMagic]],
  [["__MACOSX/Title/._main.elf", false, wrongVersion]],
  [["__MACOSX/Title/._main.elf", false, metadata.subarray(0, 25)]],
  [["__MACOSX/Title/._main.elf", false, oversizedTable]],
  ...invalidDescriptors.map(bytes => [["__MACOSX/Title/._main.elf", false, bytes]]),
  [["__MACOSX/Title/._absent.elf", false, metadata]],
  [...sidecars, ["__MACOSX/Title/readme", false, new Uint8Array([1])]],
  [...sidecars, ["__MACOSX/unrelated/", true]],
  [...sidecars, ["other/resource", false, new Uint8Array([1])]]
]) {
  const archive = new Archive(await zipped([...app, ...extra]));
  try { await assert.rejects(archive.inspect(() => {}), /exactly one executable ELF/); }
  finally { await archive.close(); }
}

for (const [offset, length] of [[0, 0], [50, 0], [86, 0]]) {
  const emptyEntry = metadata.slice();
  new DataView(emptyEntry.buffer).setUint32(30, offset);
  new DataView(emptyEntry.buffer).setUint32(34, length);
  const archive = new Archive(await zipped([["main.elf", false], ["__MACOSX/._main.elf", false, emptyEntry]]));
  try {
    assert.equal((await archive.inspect(() => {})).files, 1);
    assert.deepEqual(archive.files.map(item => item.name), ["main.elf"]);
  } finally { await archive.close(); }
}

for (const bytes of [wrongMagic, wrongVersion, metadata.subarray(0, 25), oversizedTable, ...invalidDescriptors]) {
  const name = "__MACOSX/._main.elf";
  const archive = new Archive(await zipped([["main.elf", false], [name, false, bytes]]));
  try {
    assert.equal((await archive.inspect(() => {})).files, 2);
    assert.deepEqual(archive.files.map(item => item.name), ["main.elf", name]);
  } finally { await archive.close(); }
}

for (const unrelated of [
  ["__MACOSX/readme", false, new Uint8Array([1])], ["__MACOSX/unrelated/", true],
  ["__MACOSX/__MACOSX/._._main.elf", false, metadata], ["__MACOSX/__MACOSX/", true]
]) {
  const archive = new Archive(await zipped([["main.elf", false], ["__MACOSX/._main.elf", false, metadata], unrelated]));
  try {
    await archive.inspect(() => {});
    assert.deepEqual(archive.files.map(item => item.name), ["main.elf", "__MACOSX/._main.elf", unrelated[0]]);
  } finally { await archive.close(); }
}

const knownDirectory = new Archive(await zipped([
  ["main.elf", false], ["assets/", true], ["__MACOSX/._main.elf", false, metadata], ["__MACOSX/assets/", true]
]));
try {
  await knownDirectory.inspect(() => {});
  assert.deepEqual(knownDirectory.files.map(item => item.name), ["main.elf", "assets/"]);
} finally { await knownDirectory.close(); }

for (const [entries, message] of [
  [[...app, ["__MACOSX/../._main.elf", false, metadata]], /Unsafe ZIP path/],
  [[...app, ...sidecars, ["__MACOSX/title/._MAIN.ELF", false, metadata]], /Duplicate ZIP path/],
  [[...app, ["__MACOSX/Title/._main.elf", false, metadata, { password: "metadata-password" }]], /Encrypted ZIP entries/],
  [[...app, ["__MACOSX/Title/._main.elf", false, metadata, { versionMadeBy: 0x31e, externalFileAttributes: (0o120777 << 16) >>> 0 }]], /Links and special files/],
  [[["Title/", true], ["__MACOSX/._Title", false, metadata]], /No files were selected/]
]) {
  const archive = new Archive(await zipped(entries));
  try { await assert.rejects(archive.inspect(() => {}), message); }
  finally { await archive.close(); }
}

const parents = ["outer/", "outer/inner/", "outer/inner/title/"];
for (const directories of [[], parents, [...parents].reverse()]) {
  const archive = new Archive(await zipped([
    ...directories.map(name => [name, true]), ["outer/inner/title/main.elf", false]
  ]));
  try {
    const summary = await archive.inspect(() => {});
    assert.equal(summary.root, "outer/inner/title/");
    assert.equal(summary.name, "main.elf");
    assert.equal(summary.files, 1);
    assert.deepEqual((await archive.conversionInput("linux", false, () => {})).executable, header);
  } finally { await archive.close(); }
}

const folder = new Archive([{ path: "outer/inner/title/main.elf", file: new Blob([header]) }]);
try { assert.equal((await folder.inspect(() => {})).root, "outer/inner/title/"); }
finally { await folder.close(); }

const siblings = new Archive(await zipped([["outer/", true], ["outer/main.elf", false], ["other/resource", false]]));
try { await assert.rejects(siblings.inspect(() => {}), /exactly one executable ELF/); }
finally { await siblings.close(); }

for (const cleanupRejects of [false, true]) {
  const failure = new Error("input read failed");
  let aborted = false;
  const output = new WritableStream({ abort(reason) {
    aborted = true;
    assert.equal(reason, failure);
    if (cleanupRejects) throw new Error("abort failed");
  } });
  const source = new ReadableStream({ start(controller) { controller.error(failure); } });
  const writer = new DirectoryWriter({ async file() { return output; } });
  await assert.rejects(writer.add("example.bin", source), error => error === failure);
  assert.equal(aborted, true);
  assert.equal(output.locked, false);
  assert.equal(source.locked, false);
}

{
  const failure = new Error("progress failed");
  let cancelled = false;
  let aborted = false;
  const source = new ReadableStream({
    start(controller) { controller.enqueue(new Uint8Array([1])); },
    cancel(reason) { cancelled = true; assert.equal(reason, failure); throw new Error("cancel failed"); }
  });
  const output = new WritableStream({ abort(reason) { aborted = true; assert.equal(reason, failure); } });
  const writer = new DirectoryWriter({ async file() { return output; } });
  await assert.rejects(writer.add("example.bin", source, { onprogress() { throw failure; } }), error => error === failure);
  assert.equal(cancelled, true);
  assert.equal(aborted, true);
  assert.equal(output.locked, false);
  assert.equal(source.locked, false);
}

{
  const failure = new Error("blob read failed");
  let aborted = false;
  const output = new WritableStream({ abort(reason) {
    aborted = true;
    assert.equal(reason, failure);
    throw new Error("abort failed");
  } });
  const source = { size: 1, async init() {}, async readUint8Array() { throw failure; } };
  const writer = new DirectoryWriter({ async file() { return output; } });
  await assert.rejects(writer.add("example.bin", source), error => error === failure);
  assert.equal(aborted, true);
  assert.equal(output.locked, false);
}
