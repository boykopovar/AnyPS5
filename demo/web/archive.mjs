import { BlobReader, Uint8ArrayReader, ZipReader, ZipWriter, configure } from "https://cdn.jsdelivr.net/npm/@zip.js/zip.js@2.7.57/lib/zip-no-worker.js/+esm";
import { batchMemoryEstimate, batchOutputDirectory, MEMORY_LIMIT, memoryEstimate } from "./batch.mjs";

configure({ useWebWorkers: false, useCompressionStream: false, chunkSize: 65536 });

export { batchMemoryEstimate, batchOutputDirectory, memoryEstimate, MEMORY_LIMIT };
export const CONVERSION_LIMIT = 256 * 1024 * 1024;
export const ICON_LIMIT = 16 * 1024 * 1024;
const MAX_ENTRIES = 100000;

class BoundedBlobReader extends BlobReader {
  async readUint8Array(offset, length) {
    if (length > 16 * 1024 * 1024) throw new Error("ZIP directory metadata exceeds 16 MiB");
    return super.readUint8Array(offset, length);
  }
}


export function validPath(name) {
  if (!name || name.includes("\\") || /[\x00-\x1f<>:"|?*]/u.test(name)) throw new Error(`Unsafe ZIP path: ${name}`);
  const parts = name.replace(/\/$/u, "").split("/");
  if (parts.some(part => !part || part === "." || part === ".." || /[. ]$/u.test(part) || /^(con|prn|aux|nul|com[1-9\u00b9\u00b2\u00b3]|lpt[1-9\u00b9\u00b2\u00b3])(?:\.|$)/iu.test(part))) throw new Error(`Unsafe ZIP path: ${name}`);
  return name;
}

function safeSize(value) {
  if (!Number.isSafeInteger(value) || value < 0) throw new Error("ZIP contains an unsupported file size");
  return value;
}

async function headerBytes(entry, size = 4) {
  if (entry.uncompressedSize < size) return null;
  const stop = new AbortController();
  const header = new Uint8Array(size);
  let filled = 0;
  try {
    await entry.getData(new WritableStream({
      write(chunk) {
        const length = Math.min(size - filled, chunk.length);
        header.set(chunk.subarray(0, length), filled);
        filled += length;
        if (filled === size) stop.abort();
      }
    }), { signal: stop.signal, checkSignature: false });
  } catch (error) {
    if (!stop.signal.aborted || filled !== size) throw error;
  }
  return filled === size ? header : null;
}

async function isAppleDouble(entry) {
  const header = await headerBytes(entry, 26);
  const view = header && new DataView(header.buffer, header.byteOffset, header.byteLength);
  if (!view || view.getUint32(0) !== 0x00051607 || view.getUint32(4) !== 0x00020000) return false;
  const size = 26 + view.getUint16(24) * 12;
  const table = size === 26 ? header : await headerBytes(entry, size);
  if (!table) return false;
  const descriptors = new DataView(table.buffer, table.byteOffset, table.byteLength);
  const identifiers = new Set();
  for (let position = 26; position < size; position += 12) {
    const identifier = descriptors.getUint32(position);
    const offset = descriptors.getUint32(position + 4);
    const length = descriptors.getUint32(position + 8);
    if (identifier === 0 || identifier === 1 || identifiers.has(identifier)
        || offset > entry.uncompressedSize || length > entry.uncompressedSize - offset
        || ((offset !== 0 || length !== 0) && offset < size)) return false;
    identifiers.add(identifier);
  }
  return true;
}

const SELF_MAGICS = [
  [0x4f, 0x15, 0x3d, 0x1d],
  [0x54, 0x14, 0xf5, 0xee]
];

function isElf(header) {
  return header !== null && header[0] === 0x7f && header[1] === 69 && header[2] === 76 && header[3] === 70;
}

function isSelf(header) {
  return header !== null && SELF_MAGICS.some(magic => magic.every((byte, index) => header[index] === byte));
}

async function boundedBytes(entry, limit) {
  if (entry.uncompressedSize > limit) throw new Error(`${entry.filename} exceeds the conversion memory limit`);
  let size = 0;
  const chunks = [];
  await entry.getData(new WritableStream({
    write(chunk) {
      size += chunk.byteLength;
      if (size > limit || size > entry.uncompressedSize) throw new Error(`Unexpected expanded size: ${entry.filename}`);
      chunks.push(chunk);
    }
  }), { checkSignature: true });
  if (size !== entry.uncompressedSize) throw new Error(`Incorrect expanded size: ${entry.filename}`);
  const bytes = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
  return bytes;
}

class FolderReader {
  constructor(files) {
    this.files = files;
  }

  async *getEntriesGenerator() {
    for (const { path, file } of this.files) {
      if (!(file instanceof Blob)) throw new Error(`Invalid folder file: ${path}`);
      yield {
        filename: path, uncompressedSize: file.size, directory: false, compressionMethod: 0,
        externalFileAttributes: (0o100644 << 16) >>> 0,
        getData(destination, options = {}) {
          return file.stream().pipeTo(destination, { signal: options.signal });
        }
      };
    }
  }

  async close() {}
}

export class Archive {
  constructor(file) {
    this.reader = Array.isArray(file) ? new FolderReader(file) : new ZipReader(new BoundedBlobReader(file));
    this.entries = [];
  }

  async inspect(report) {
    const paths = new Map();
    let total = 0;
    for await (const entry of this.reader.getEntriesGenerator()) {
      if (this.entries.length >= MAX_ENTRIES) throw new Error("Input contains more than 100,000 entries");
      if (Number.isInteger(entry.versionMadeBy) && entry.versionMadeBy >> 8 === 0 && entry.filename.includes("\\")) {
        entry.filename = entry.filename.replaceAll("\\", "/");
        if (entry.filename.endsWith("/")) entry.directory = true;
      }
      validPath(entry.filename);
      const key = entry.filename.replace(/\/$/u, "").toLowerCase();
      if (paths.has(key)) throw new Error(`Duplicate ZIP path: ${entry.filename}`);
      if (entry.encrypted) throw new Error(`Encrypted ZIP entries are not supported: ${entry.filename}`);
      if (!entry.directory && entry.compressionMethod !== 0 && entry.compressionMethod !== 8) throw new Error(`Unsupported ZIP compression: ${entry.filename}`);
      const mode = (entry.externalFileAttributes >>> 16) & 0xf000;
      if (mode && mode !== 0x8000 && mode !== 0x4000) throw new Error(`Links and special files are not supported: ${entry.filename}`);
      paths.set(key, entry.directory);
      total = safeSize(total + safeSize(entry.uncompressedSize));
      this.entries.push(entry);
    }
    if (!this.entries.some(entry => !entry.directory)) throw new Error("No files were selected");
    const directories = new Set();
    for (const entry of this.entries) {
      const parts = entry.filename.split("/");
      for (let index = 1; index < parts.length; index++) {
        const parent = parts.slice(0, index).join("/").toLowerCase();
        if (paths.get(parent) === false) throw new Error(`A file is also used as a folder: ${entry.filename}`);
        if (!entry.filename.toLowerCase().startsWith("__macosx/")) directories.add(parent);
      }
    }
    const applicationPaths = new Map([...paths].filter(([name]) => name !== "__macosx" && !name.startsWith("__macosx/")));
    const metadata = this.entries.filter(entry => entry.filename.startsWith("__MACOSX/"));
    let appleDouble = metadata.some(entry => !entry.directory);
    for (const entry of metadata) {
      if (entry.directory) {
        const companion = entry.filename.slice("__MACOSX/".length).replace(/\/$/u, "").toLowerCase();
        if (companion && applicationPaths.get(companion) !== true && !directories.has(companion)) { appleDouble = false; break; }
        continue;
      }
      const match = /^__MACOSX\/(.*\/)?\._([^/]+)$/u.exec(entry.filename);
      const companion = match && ((match[1] ?? "") + match[2]).toLowerCase();
      if (!companion || (!applicationPaths.has(companion) && !directories.has(companion)) || !await isAppleDouble(entry)) { appleDouble = false; break; }
    }
    if (appleDouble) this.entries = this.entries.filter(entry => !entry.filename.startsWith("__MACOSX/"));
    if (!this.entries.some(entry => !entry.directory)) throw new Error("No files were selected");
    let prefix = "";
    const files = this.entries.filter(entry => !entry.directory);
    while (files.every(entry => entry.filename.slice(prefix.length).includes("/"))) {
      const first = files[0].filename.slice(prefix.length).split("/")[0];
      if (["sce_sys", "sce_module", "sce_modules", "prx"].includes(first) || !this.entries.every(entry => (entry.directory && prefix.startsWith(entry.filename)) || entry.filename.startsWith(prefix + first + "/"))) break;
      prefix += first + "/";
    }
    this.prefix = prefix;
    this.files = this.entries.filter(entry => entry.filename.startsWith(prefix) && entry.filename !== prefix).map(entry => ({ entry, name: entry.filename.slice(prefix.length) }));
    const moduleDirs = ["sce_module", "sce_modules", "prx"];
    const present = directory => this.files.some(item => item.name.startsWith(directory + "/"));
    if (present("sce_module") && present("sce_modules")) throw new Error("Both sce_module and sce_modules exist in the app root");
    const executables = [];
    this.modules = [];
    const selfContainers = [];
    const selfModules = [];
    for (let index = 0; index < this.files.length; index++) {
      const item = this.files[index];
      report({ phase: "Inspecting files", input: item.name, completed: index, total: this.files.length });
      if (item.entry.directory || item.name.startsWith("sce_sys/")) continue;
      const moduleDir = moduleDirs.find(directory => item.name.startsWith(directory + "/"));
      if (!moduleDir && item.name.includes("/")) continue;
      if (moduleDir && item.name.endsWith(".guest.prx")) continue;
      const header = await headerBytes(item.entry);
      if (moduleDir) {
        if (isElf(header)) this.modules.push(item);
        else if (isSelf(header)) selfModules.push(item.name);
      } else if (isElf(header)) {
        executables.push(item);
      } else if (isSelf(header)) {
        selfContainers.push(item.name);
      }
    }
    if (executables.length !== 1) {
      if (selfContainers.length > 0) throw new Error(`${selfContainers.join(", ")} is a SELF container, not an ELF: unwrap the app before converting it`);
      throw new Error(`Expected exactly one executable ELF in the app root; found ${executables.length}`);
    }
    this.executable = executables[0];
    if (this.executable.name.includes("/")) throw new Error("Place the executable next to sce_sys and sce_module, not inside another folder");
    this.icon = this.files.find(item => item.name === "sce_sys/icon0.png" && !item.entry.directory);
    this.selectedBytes = [this.executable, ...this.modules].reduce((sum, item) => sum + item.entry.uncompressedSize, 0);
    this.windowsSelectedBytes = this.selectedBytes + (this.icon?.entry.uncompressedSize ?? 0);
    if (this.selectedBytes > CONVERSION_LIMIT) throw new Error("Executable and modules exceed the 256 MiB conversion limit. Use the native relinker for this app.");
    const warnings = [];
    if (!this.files.some(item => item.name.startsWith("sce_sys/"))) warnings.push("sce_sys is missing. The Windows executable will have no app icon.");
    else if (!this.icon) warnings.push("sce_sys/icon0.png is missing. The Windows executable will have no app icon.");
    if (!moduleDirs.some(present)) warnings.push("sce_module, sce_modules and prx are missing. If the original app included a module folder, leaving it out can prevent the app from starting.");
    if (selfModules.length > 0) warnings.push(`${selfModules.join(", ")} is a SELF container and was skipped: unwrap it to include that module.`);
    this.totalBytes = total;
    return { name: this.executable.name, files: files.length, modules: this.modules.length, totalBytes: total, selectedBytes: this.selectedBytes, windowsSelectedBytes: this.windowsSelectedBytes, windowsIconTooLarge: (this.icon?.entry.uncompressedSize ?? 0) > ICON_LIMIT, root: prefix, warnings };
  }

  selectedBytesForTarget(target) {
    if (target !== "linux" && target !== "windows") throw new Error(`Unsupported conversion target: ${target}`);
    if (target === "windows" && (this.icon?.entry.uncompressedSize ?? 0) > ICON_LIMIT) throw new Error("sce_sys/icon0.png exceeds 16 MiB");
    return target === "windows" ? this.windowsSelectedBytes : this.selectedBytes;
  }

  async conversionInput(target, toIntel, report) {
    let remaining = CONVERSION_LIMIT;
    const selectedBytes = this.selectedBytesForTarget(target);
    if (selectedBytes > CONVERSION_LIMIT) throw new Error(`Selected ${target} conversion input exceeds the 256 MiB conversion limit. Use the native relinker for this app.`);
    const read = async item => {
      report({ phase: "Reading conversion input", input: item.name });
      const bytes = await boundedBytes(item.entry, remaining);
      remaining -= bytes.length;
      return bytes;
    };
    const name = this.executable.name;
    const stem = name.includes(".") ? name.slice(0, name.lastIndexOf(".")) : name;
    const outputName = (stem || "app") + (target === "windows" ? ".exe" : ".elf");
    const input = { name, outputName, target, toIntel, executable: await read(this.executable), modules: [], icon: null };
    for (const item of this.modules) input.modules.push({ name: item.name, bytes: await read(item) });
    if (this.icon && target === "windows") input.icon = await read(this.icon);
    return input;
  }

  async close() { await this.reader.close(); }
}

export function createPackageWriter(destination) {
  return new ZipWriter(destination, { level: 9, zip64: true, bufferedWrite: false, useWebWorkers: false, useCompressionStream: false, preventClose: true });
}

async function appendEntry(writer, name, entry, report) {
  const stream = new TransformStream();
  let produced = 0;
  const limit = new TransformStream({ transform(chunk, controller) {
    produced += chunk.length;
    if (produced > entry.uncompressedSize) throw new Error(`Unexpected expanded size: ${name}`);
    controller.enqueue(chunk);
  }, flush() {
    if (produced !== entry.uncompressedSize) throw new Error(`Incorrect expanded size: ${name}`);
  } });
  await Promise.all([
    entry.getData(stream.writable, { checkSignature: true }),
    writer.add(name, stream.readable.pipeThrough(limit), {
      versionMadeBy: 0x31e, externalFileAttributes: entry.externalFileAttributes >>> 16 ? entry.externalFileAttributes : ((0o100644 << 16) | (entry.externalFileAttributes & 0xffff)) >>> 0,
      onprogress: completed => report({ phase: writer instanceof DirectoryWriter ? "Writing files" : "Compressing ZIP", input: name, completed, total: entry.uncompressedSize })
    })
  ]);
}

export class DirectoryWriter {
  constructor(destination) {
    this.destination = destination;
  }

  async add(name, source, options = {}) {
    if (options.directory) {
      await this.destination.directory(name);
      return;
    }
    if (!(source instanceof ReadableStream)) await source.init();
    const output = await this.destination.file(name);
    if (source instanceof ReadableStream) {
      let completed = 0;
      const reader = source.getReader();
      const writer = output.getWriter();
      try {
        for (;;) {
          const { value, done } = await reader.read();
          if (done) break;
          for (let offset = 0; offset < value.length; offset += 65536) {
            const chunk = value.subarray(offset, offset + 65536);
            await writer.write(chunk);
            completed += chunk.length;
            options.onprogress?.(completed);
          }
        }
        await writer.close();
      } catch (error) {
        await Promise.allSettled([reader.cancel(error), writer.abort(error)]);
        throw error;
      } finally {
        reader.releaseLock();
        writer.releaseLock();
      }
    } else {
      const writer = output.getWriter();
      try {
        for (let offset = 0; offset < source.size; offset += 65536) {
          await writer.write(await source.readUint8Array(offset, Math.min(65536, source.size - offset)));
        }
        await writer.close();
      } catch (error) {
        await Promise.allSettled([writer.abort(error)]);
        throw error;
      } finally {
        writer.releaseLock();
      }
    }
  }

  async close() {}
}

export async function packageArchive(archive, converted, outputName, libraryFile, destination, report, directory = false, options = {}) {
  const libraries = new ZipReader(new BoundedBlobReader(libraryFile));
  const ownsWriter = !options.writer;
  const writer = options.writer ?? (directory ? new DirectoryWriter(destination) : createPackageWriter(destination));
  const outputPath = name => {
    const path = options.prefix ? `${options.prefix}${name}` : name;
    validPath(path);
    return path;
  };
  const names = new Set();
  const claim = name => {
    const path = outputPath(name);
    const key = path.replace(/\/$/u, "").toLowerCase();
    if (names.has(key)) throw new Error(`Output path collision: ${name}`);
    names.add(key);
  };
  try {
    const libraryEntries = await libraries.getEntries();
    if (!libraryEntries.length || libraryEntries.length > 4096) throw new Error("Invalid library archive");
    for (const entry of libraryEntries) {
      if (!entry.filename.startsWith("libs/") || entry.encrypted || ((entry.externalFileAttributes >>> 16) & 0xf000) === 0xa000) throw new Error("Invalid library archive layout");
      if (entry.compressionMethod !== 0 && entry.compressionMethod !== 8) throw new Error(`Unsupported ZIP compression: ${entry.filename}`);
      const mode = (entry.externalFileAttributes >>> 16) & 0xf000;
      if (mode && mode !== 0x8000 && mode !== 0x4000) throw new Error(`Links and special files are not supported: ${entry.filename}`);
      claim(entry.filename);
    }
    const replaced = new Set(converted.modules.map(module => module.name));
    const originalFiles = archive.files.filter(item => !replaced.has("app0/" + item.name));
    for (const item of originalFiles) claim("app0/" + item.name);
    claim(outputName);
    if (converted.diagnostics) claim("windows-diagnostics-imports.txt");
    for (const module of converted.modules) {
      if (!["app0/sce_module/", "app0/sce_modules/", "app0/prx/"].some(prefix => module.name.startsWith(prefix))) throw new Error("Invalid converted module path");
      claim(module.name);
    }
    const outputEntries = [
      ...libraryEntries.map(entry => ({ name: entry.filename, directory: entry.directory })),
      ...originalFiles.map(item => ({ name: "app0/" + item.name, directory: item.entry.directory })),
      { name: outputName },
      ...(converted.diagnostics ? [{ name: "windows-diagnostics-imports.txt" }] : []),
      ...converted.modules.map(module => ({ name: module.name }))
    ];
    const files = new Set(outputEntries.filter(entry => !entry.directory).map(entry => entry.name.toLowerCase()));
    for (const { name } of outputEntries) {
      const parts = name.replace(/\/$/u, "").toLowerCase().split("/");
      for (let index = 1; index < parts.length; index++) {
        const parent = parts.slice(0, index).join("/");
        if (files.has(parent)) throw new Error(`Output path collision: ${parent}`);
      }
    }
    const binary = { versionMadeBy: 0x31e, externalFileAttributes: (0o100755 << 16) >>> 0 };
    await writer.add(outputPath(outputName), new Uint8ArrayReader(converted.executable), binary);
    if (converted.diagnostics) await writer.add(outputPath("windows-diagnostics-imports.txt"), new Uint8ArrayReader(new TextEncoder().encode(converted.diagnostics)));
    for (const module of converted.modules) await writer.add(outputPath(module.name), new Uint8ArrayReader(module.bytes), binary);
    for (const entry of libraryEntries) {
      if (entry.directory) await writer.add(outputPath(entry.filename), undefined, { directory: true });
      else await appendEntry(writer, outputPath(entry.filename), entry, report);
    }
    for (const { entry, name } of originalFiles) {
      if (entry.directory) await writer.add(outputPath("app0/" + name), undefined, { directory: true });
      else await appendEntry(writer, outputPath("app0/" + name), entry, report);
    }
    if (ownsWriter) await writer.close();
  } finally {
    await libraries.close();
  }
}
