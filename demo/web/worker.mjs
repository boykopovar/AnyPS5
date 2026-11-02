import { batchMemoryEstimate, batchOutputDirectory, MEMORY_LIMIT } from "./batch.mjs";
import { Archive, CONVERSION_LIMIT, createPackageWriter, packageArchive } from "./archive.mjs";

let archive;
let inputs = [];
let summaries = [];
let busy = false;
let pendingWrite;
let lastReport = 0;
let lastPhase = "";

function report(event) {
  const phase = event.phase ?? String(event.stage);
  const now = performance.now();
  if (!event.message && phase === lastPhase && event.completed !== event.total && now - lastReport < 80) return;
  lastReport = now;
  lastPhase = phase;
  self.postMessage({ type: "progress", ...event });
}

function reportFor(item, index) {
  let started = false;
  return event => {
    if (!started) {
      lastReport = 0;
      lastPhase = "";
      started = true;
    }
    report({ ...event, appIndex: index, appCount: inputs.length, appName: item.name });
  };
}

export async function downloadLibraries(target) {
  report({ phase: "Downloading system libraries" });
  const response = await fetch("./release.json", { cache: "no-cache" });
  if (!response.ok) throw new Error(`Cannot load release manifest (${response.status})`);
  const release = await response.json();
  const asset = release.platforms[target];
  if (!asset || !Number.isSafeInteger(asset.size) || asset.size < 0 || asset.size > 512 * 1024 * 1024 || !/^libraries\/(windows|linux)-[a-f0-9]+\.zip$/u.test(asset.path) || !/^[a-f0-9]{64}$/u.test(asset.sha256)) throw new Error("Invalid release manifest");
  const latestResponse = await fetch("https://api.github.com/repos/boykopovar/AnyPS5/releases/latest", { headers: { Accept: "application/vnd.github+json" } });
  if (!latestResponse.ok) {
    const rateLimited = latestResponse.status === 429 || (latestResponse.status === 403 && latestResponse.headers.get("x-ratelimit-remaining") === "0");
    if (!rateLimited) throw new Error(`Cannot verify the latest release (${latestResponse.status}). Try again later.`);
    report({ phase: "Downloading system libraries", message: "GitHub API rate limit reached; using the libraries published with this site." });
  } else {
    const latest = await latestResponse.json();
    if (latest.tag_name !== release.tag) throw new Error("A new release is being published to this site. Try again after the Pages build finishes.");
  }
  const download = await fetch(asset.path);
  if (!download.ok) throw new Error(`Cannot download system libraries (${download.status})`);
  const reader = download.body.getReader();
  const chunks = [];
  let size = 0;
  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      size += value.byteLength;
      if (size > asset.size) throw new Error("System library download is incomplete");
      report({ phase: "Downloading system libraries", completed: size, total: asset.size });
      chunks.push(value);
    }
    if (size !== asset.size) throw new Error("System library download is incomplete");
  } catch (error) {
    await reader.cancel(error).catch(() => {});
    throw error;
  } finally {
    reader.releaseLock();
  }
  const bytes = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.byteLength; }
  const digest = [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))].map(byte => byte.toString(16).padStart(2, "0")).join("");
  if (digest !== asset.sha256) throw new Error("System library checksum mismatch");
  return { file: new Blob([bytes]), release, expandedBytes: asset.expandedBytes };
}

async function convertInput(item, index, native, libraries, data, destination, directory, writer) {
  const current = archive ?? new Archive(item.files ?? item.file);
  archive = null;
  try {
    const itemReport = reportFor(item, index);
    if (inputs.length > 1) await current.inspect(itemReport);
    const input = await current.conversionInput(data.target, data.toIntel, itemReport);
    const converted = native.Convert(input, itemReport);
    if (!converted.ok) throw new Error(converted.error);
    const options = inputs.length > 1 ? { prefix: `${batchOutputDirectory(item.name, index)}/`, writer } : {};
    await packageArchive(current, converted, input.outputName, libraries.file, destination, itemReport, directory, options);
    return input.outputName;
  } catch (error) {
    throw new Error(`${item.name}: ${error instanceof Error ? error.message : String(error)}`);
  } finally {
    await current.close();
  }
}

self.onmessage = async ({ data }) => {
  if (data.type === "written") {
    if (!pendingWrite) return;
    const resolve = pendingWrite;
    pendingWrite = null;
    resolve();
    return;
  }
  if (busy) return;
  busy = true;
  try {
    if (data.type === "inspect") {
      if (archive) await archive.close();
      archive = null;
      inputs = data.inputs ?? [{ name: data.file?.name ?? "selected app", file: data.file, files: data.files }];
      if (!inputs.length) throw new Error("Choose at least one ZIP archive.");
      summaries = [];
      for (let index = 0; index < inputs.length; index++) {
        const item = inputs[index];
        const current = new Archive(item.files ?? item.file);
        let keepOpen = false;
        try {
          const summary = await current.inspect(reportFor(item, index));
          summaries.push(summary);
          if (inputs.length === 1) {
            archive = current;
            keepOpen = true;
          }
          self.postMessage({ type: "inspected", index, count: inputs.length, name: item.name, summary });
        } catch (error) {
          throw new Error(`${item.name}: ${error instanceof Error ? error.message : String(error)}`);
        } finally {
          if (!keepOpen) await current.close();
        }
      }
      self.postMessage({ type: "inspection-complete", count: inputs.length });
    } else if (data.type === "convert") {
      if (data.target !== "linux" && data.target !== "windows") throw new Error(`Unsupported conversion target: ${data.target}`);
      if (!["memory", "zip", "disk"].includes(data.mode)) throw new Error("Unknown save mode");
      if (data.target === "windows" && data.mode === "disk") throw new Error("Windows folder output is unavailable because browsers block DLL creation. Choose a ZIP save mode.");
      if (!inputs.length || summaries.length !== inputs.length) throw new Error("Inspect the selected apps before converting.");
      if (inputs.length > 1 && data.mode === "disk") throw new Error("Batch conversion requires a ZIP save mode.");
      for (const summary of summaries) {
        const selectedBytes = data.target === "windows" ? summary.windowsSelectedBytes : summary.selectedBytes;
        if (data.target === "windows" && summary.windowsIconTooLarge) throw new Error("A Windows icon exceeds 16 MiB. Choose Linux or use the native relinker.");
        if (selectedBytes > CONVERSION_LIMIT) throw new Error(`Selected ${data.target} conversion input exceeds the 256 MiB conversion limit. Use the native relinker for this app.`);
      }
      const libraries = await downloadLibraries(data.target);
      if (data.mode === "memory" && batchMemoryEstimate(summaries, data.target, libraries.expandedBytes) > MEMORY_LIMIT) throw new Error("This result exceeds the 256 MiB memory-download budget. Use Save ZIP to disk in desktop Chrome or Edge.");
      report({ phase: "Loading converter" });
      const { default: createRelinker } = await import("./native/relinker_wasm.mjs");
      const native = await createRelinker();
      const chunks = [];
      let size = 0;
      let outputName = "anyps5-apps";
      const send = (event, transfer = []) => new Promise(resolve => {
        pendingWrite = resolve;
        self.postMessage(event, transfer);
      });
      if (data.mode === "disk") {
        const destination = {
          directory: name => send({ type: "directory", name }),
          async file(name) {
            await send({ type: "open-file", name });
            return new WritableStream({
              async write(chunk) {
                size += chunk.byteLength;
                const bytes = chunk.slice();
                await send({ type: "write", bytes }, [bytes.buffer]);
              },
              close: () => send({ type: "close-file" })
            });
          }
        };
        outputName = await convertInput(inputs[0], 0, native, libraries, data, destination, true);
      } else {
        const destination = new WritableStream({
          async write(chunk) {
            size += chunk.byteLength;
            if (data.mode === "zip") {
              const bytes = chunk.slice();
              await send({ type: "write", bytes }, [bytes.buffer]);
              return;
            }
            if (size > MEMORY_LIMIT) throw new Error("The compressed ZIP exceeded 256 MiB. Use Save ZIP to disk.");
            chunks.push(chunk.slice());
          }
        });
        const writer = inputs.length > 1 ? createPackageWriter(destination) : null;
        for (let index = 0; index < inputs.length; index++) {
          const name = await convertInput(inputs[index], index, native, libraries, data, destination, false, writer);
          if (inputs.length === 1) outputName = name;
        }
        if (writer) await writer.close();
        await destination.close();
      }
      self.postMessage({ type: "complete", mode: data.mode, blob: data.mode === "memory" ? new Blob(chunks, { type: "application/zip" }) : null, outputName, size, release: libraries.release.tag, apps: inputs.length });
      inputs = [];
      summaries = [];
    } else throw new Error("Unknown worker request");
  } catch (error) {
    self.postMessage({ type: "error", message: error instanceof TypeError ? "Cannot reach the release files: check the connection and try again." : error instanceof Error ? error.message : String(error) });
  } finally {
    busy = false;
  }
};
