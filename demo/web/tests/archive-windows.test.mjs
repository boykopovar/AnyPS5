import assert from "node:assert/strict";
import { register } from "node:module";
import { fileURLToPath, pathToFileURL } from "node:url";
import path from "node:path";

register("./cdn-loader.mjs", import.meta.url, { data: { url: process.env.ANYPS5_TEST_ZIP_URL, path: process.env.ANYPS5_TEST_ZIP_PATH } });
const webRoot = process.argv[2] ?? fileURLToPath(new URL("..", import.meta.url));
const { Archive, packageArchive, validPath } = await import(pathToFileURL(path.join(webRoot, "archive.mjs")));
const { BlobReader, BlobWriter, Uint8ArrayReader, ZipReader, ZipWriter } = await import(process.env.ANYPS5_TEST_ZIP_URL);
const header = new Uint8Array([127, 69, 76, 70]);
const dos = { versionMadeBy: 0x14, externalFileAttributes: 0x20 };

async function zipped(entries, options) {
  const writer = new ZipWriter(new BlobWriter(), { useWebWorkers: false, useCompressionStream: false });
  for (const [name, directory] of entries) {
    if (directory) await writer.add(name, new Uint8ArrayReader(new Uint8Array()), { ...options, externalFileAttributes: 0x10 });
    else await writer.add(name, new Uint8ArrayReader(header), options);
  }
  return writer.close();
}

const windows = new Archive(await zipped([["title\\main.elf", false], ["title\\sce_sys\\param.json", false], ["title\\data\\", true]], dos));
const output = [];
try {
  const summary = await windows.inspect(() => {});
  assert.equal(summary.root, "title/");
  assert.equal(summary.name, "main.elf");
  assert.deepEqual(windows.files.map(item => item.name).sort(), ["data/", "main.elf", "sce_sys/param.json"]);
  const libraries = new ZipWriter(new BlobWriter(), { useWebWorkers: false, useCompressionStream: false });
  await libraries.add("libs/libc.prx", new Uint8ArrayReader(header), { versionMadeBy: 0x31e, externalFileAttributes: (0o100644 << 16) >>> 0 });
  await packageArchive(windows, { executable: header, modules: [] }, "main.elf", await libraries.close(), new WritableStream({ write(chunk) { output.push(chunk.slice()); } }), () => {});
} finally { await windows.close(); }
const packaged = new ZipReader(new BlobReader(new Blob(output)));
try {
  const modes = Object.fromEntries((await packaged.getEntries()).filter(entry => !entry.directory).map(entry => [entry.filename, entry.externalFileAttributes >>> 16]));
  assert.equal(modes["app0/main.elf"], 0o100644);
  assert.equal(modes["app0/sce_sys/param.json"], 0o100644);
  assert.equal(modes["main.elf"], 0o100755);
} finally { await packaged.close(); }

const escape = new Archive(await zipped([["title\\main.elf", false], ["title\\..\\..\\evil", false]], dos));
try { await assert.rejects(escape.inspect(() => {}), /Unsafe ZIP path: title\/\.\.\/\.\.\/evil/); }
finally { await escape.close(); }

const unix = new Archive(await zipped([["title\\main.elf", false]], { versionMadeBy: 0x31e, externalFileAttributes: (0o100644 << 16) >>> 0 }));
try { await assert.rejects(unix.inspect(() => {}), /Unsafe ZIP path: title\\main\.elf/); }
finally { await unix.close(); }

const maximum = "a".repeat(255);
assert.equal(validPath(maximum), maximum);
assert.equal(validPath("data/" + maximum), "data/" + maximum);
assert.throws(() => validPath("a".repeat(256)), /Unsafe ZIP path/);
assert.throws(() => validPath("data/" + "a".repeat(256)), /Unsafe ZIP path/);
assert.equal(validPath("\u4e2d".repeat(85)), "\u4e2d".repeat(85));
assert.throws(() => validPath("\u4e2d".repeat(86)), /Unsafe ZIP path/);

const longestFolder = new Archive([{ path: "main.elf", file: new Blob([header]) }, { path: "data/" + maximum, file: new Blob(["resource"]) }]);
try { await longestFolder.inspect(() => {}); }
finally { await longestFolder.close(); }

const overlongZip = new Archive(await zipped([["title/main.elf", false], ["title/" + "a".repeat(256) + "/", true]], dos));
try { await assert.rejects(overlongZip.inspect(() => {}), /Unsafe ZIP path/); }
finally { await overlongZip.close(); }

const longExecutable = "a".repeat(255);
const longSource = new Archive([{ path: longExecutable, file: new Blob([header]) }, { path: "sce_sys/param.json", file: new Blob(["{}"]) }]);
try {
  await longSource.inspect(() => {});
  const libraries = new ZipWriter(new BlobWriter(), { useWebWorkers: false, useCompressionStream: false });
  await libraries.add("libs/libc.prx", new Uint8ArrayReader(header));
  let writes = 0;
  await assert.rejects(packageArchive(longSource, { executable: header, modules: [] }, longExecutable + ".elf", await libraries.close(),
    { async file() { writes++; throw new Error("destination opened"); } }, () => {}, true), /Unsafe ZIP path/);
  assert.equal(writes, 0);
} finally { await longSource.close(); }
