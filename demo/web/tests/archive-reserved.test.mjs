import assert from "node:assert/strict";
import { register } from "node:module";
import { pathToFileURL } from "node:url";
import path from "node:path";

register("./cdn-loader.mjs", import.meta.url, { data: { url: process.env.ANYPS5_TEST_ZIP_URL, path: process.env.ANYPS5_TEST_ZIP_PATH } });
const { Archive, validPath } = await import(pathToFileURL(path.join(process.argv[2], "archive.mjs")));
const { BlobWriter, Uint8ArrayReader, ZipWriter } = await import(process.env.ANYPS5_TEST_ZIP_URL);
const header = new Uint8Array([127, 69, 76, 70]);

for (const prefix of ["COM", "lpt"]) {
  for (const digit of ["\u00b9", "\u00b2", "\u00b3"]) {
    for (const name of [prefix + digit, prefix + digit + ".txt", "content/" + prefix + digit + "/data.bin"]) {
      assert.throws(() => validPath(name), /Unsafe ZIP path/);
      const folder = new Archive([{ path: "main.elf", file: new Blob([header]) }, { path: name, file: new Blob(["resource"]) }]);
      try { await assert.rejects(folder.inspect(() => {}), /Unsafe ZIP path/); }
      finally { await folder.close(); }
    }
  }
}

const writer = new ZipWriter(new BlobWriter(), { useWebWorkers: false, useCompressionStream: false });
await writer.add("main.elf", new Uint8ArrayReader(header));
await writer.add("content/COM\u00b9.txt", new Uint8ArrayReader(new Uint8Array([1])));
const archive = new Archive(await writer.close());
try { await assert.rejects(archive.inspect(() => {}), /Unsafe ZIP path/); }
finally { await archive.close(); }

for (const name of ["COM10", "COM\u2074.txt", "COM\u00b9-extra", "content/ordinary.txt"]) {
  assert.equal(validPath(name), name);
}
