import { batchMemoryEstimate, batchOutputDirectory, MEMORY_LIMIT } from "./batch.mjs";
import { Archive, CONVERSION_LIMIT, createPackageWriter, packageArchive } from "./archive.mjs";
import { translate } from "./i18n.mjs";

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

export async function downloadLibraries(target, locale = "en") {
  if (target !== "linux" && target !== "windows") throw new Error(translate(locale, "invalidLibraryTarget"));
  report({ phase: translate(locale, "phaseDownloadingLibraries") });
  const response = await fetch("./release.json", { cache: "no-cache" });
  if (!response.ok) throw new Error(translate(locale, "cannotLoadManifest", { status: response.status }));
  const release = await response.json();
  const asset = release?.platforms?.[target];
  if (!release || typeof release.tag !== "string" || !/^v[0-9A-Za-z][0-9A-Za-z._-]*$/u.test(release.tag) || release.tag.endsWith(".")
      || !asset || typeof asset.sha256 !== "string" || !/^[a-f0-9]{64}$/u.test(asset.sha256)
      || asset.path !== `libraries/${target}-${asset.sha256}.zip`
      || !Number.isSafeInteger(asset.size) || asset.size <= 0 || asset.size > 512 * 1024 * 1024
      || !Number.isSafeInteger(asset.expandedBytes) || asset.expandedBytes < 0 || asset.expandedBytes > 512 * 1024 * 1024) throw new Error(translate(locale, "invalidReleaseManifest"));
  const latestResponse = await fetch("https://api.github.com/repos/boykopovar/AnyPS5/releases/latest", { headers: { Accept: "application/vnd.github+json" } });
  if (!latestResponse.ok) {
    const rateLimited = latestResponse.status === 429 || (latestResponse.status === 403 && latestResponse.headers.get("x-ratelimit-remaining") === "0");
    if (!rateLimited) throw new Error(translate(locale, "cannotVerifyRelease", { status: latestResponse.status }));
    report({ phase: translate(locale, "phaseDownloadingLibraries"), message: translate(locale, "rateLimited") });
  } else {
    const latest = await latestResponse.json();
    if (latest.tag_name !== release.tag) throw new Error(translate(locale, "releasePublishing"));
  }
  const download = await fetch(asset.path);
  if (!download.ok) throw new Error(translate(locale, "cannotDownloadLibraries", { status: download.status }));
  const reader = download.body.getReader();
  const chunks = [];
  let size = 0;
  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      size += value.byteLength;
      if (size > asset.size) throw new Error(translate(locale, "incompleteDownload"));
      report({ phase: translate(locale, "phaseDownloadingLibraries"), completed: size, total: asset.size });
      chunks.push(value);
    }
    if (size !== asset.size) throw new Error(translate(locale, "incompleteDownload"));
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
  if (digest !== asset.sha256) throw new Error(translate(locale, "checksumMismatch"));
  return { file: new Blob([bytes]), release, expandedBytes: asset.expandedBytes };
}

async function convertInput(item, index, native, libraries, data, destination, directory, writer, locale) {
  const current = archive ?? new Archive(item.files ?? item.file, locale);
  archive = null;
  try {
    const itemReport = reportFor(item, index);
    if (inputs.length > 1) await current.inspect(itemReport);
    const input = await current.conversionInput(data.target, data.toIntel, itemReport);
    const converted = native.Convert(input, itemReport);
    if (!converted.ok) throw new Error(converted.error);
    const options = inputs.length > 1 ? { prefix: `${batchOutputDirectory(item.name, index, locale)}/`, writer } : {};
    await packageArchive(current, converted, input.outputName, libraries.file, destination, itemReport, directory, options, locale);
    return input.outputName;
  } catch (error) {
    throw new Error(translate(locale, "errorPrefixed", { name: item.name, message: error instanceof Error ? error.message : String(error) }));
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
  const locale = data.locale ?? "en";
  try {
    if (data.type === "inspect") {
      if (archive) await archive.close();
      archive = null;
      inputs = data.inputs ?? [{ name: data.file?.name ?? "selected app", file: data.file, files: data.files }];
      if (!inputs.length) throw new Error(translate(locale, "chooseAtLeastOne"));
      summaries = [];
      for (let index = 0; index < inputs.length; index++) {
        const item = inputs[index];
        const current = new Archive(item.files ?? item.file, locale);
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
          throw new Error(translate(locale, "errorPrefixed", { name: item.name, message: error instanceof Error ? error.message : String(error) }));
        } finally {
          if (!keepOpen) await current.close();
        }
      }
      self.postMessage({ type: "inspection-complete", count: inputs.length });
    } else if (data.type === "convert") {
      if (data.target !== "linux" && data.target !== "windows") throw new Error(translate(locale, "unsupportedTarget", { target: data.target }));
      if (!["memory", "zip", "disk"].includes(data.mode)) throw new Error(translate(locale, "unknownSaveMode"));
      if (data.target === "windows" && data.mode === "disk") throw new Error(translate(locale, "windowsFolderUnavailable"));
      if (!inputs.length || summaries.length !== inputs.length) throw new Error(translate(locale, "inspectBeforeConvert"));
      if (inputs.length > 1 && data.mode === "disk") throw new Error(translate(locale, "batchRequiresZip"));
      for (const summary of summaries) {
        const selectedBytes = data.target === "windows" ? summary.windowsSelectedBytes : summary.selectedBytes;
        if (data.target === "windows" && summary.windowsIconTooLarge) throw new Error(translate(locale, "iconTooLargeHint"));
        if (selectedBytes > CONVERSION_LIMIT) throw new Error(translate(locale, "selectedTooLarge", { target: data.target }));
      }
      const libraries = await downloadLibraries(data.target, locale);
      if (data.mode === "memory" && batchMemoryEstimate(summaries, data.target, libraries.expandedBytes, locale) > MEMORY_LIMIT) throw new Error(translate(locale, "memoryBudgetExceeded"));
      report({ phase: translate(locale, "phaseLoadingConverter") });
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
        outputName = await convertInput(inputs[0], 0, native, libraries, data, destination, true, undefined, locale);
      } else {
        const destination = new WritableStream({
          async write(chunk) {
            size += chunk.byteLength;
            if (data.mode === "zip") {
              const bytes = chunk.slice();
              await send({ type: "write", bytes }, [bytes.buffer]);
              return;
            }
            if (size > MEMORY_LIMIT) throw new Error(translate(locale, "compressedTooLarge"));
            chunks.push(chunk.slice());
          }
        });
        const writer = inputs.length > 1 ? createPackageWriter(destination) : null;
        for (let index = 0; index < inputs.length; index++) {
          const name = await convertInput(inputs[index], index, native, libraries, data, destination, false, writer, locale);
          if (inputs.length === 1) outputName = name;
        }
        if (writer) await writer.close();
        await destination.close();
      }
      self.postMessage({ type: "complete", mode: data.mode, blob: data.mode === "memory" ? new Blob(chunks, { type: "application/zip" }) : null, outputName, size, release: libraries.release.tag, apps: inputs.length });
      inputs = [];
      summaries = [];
    } else throw new Error(translate(locale, "unknownWorkerRequest"));
  } catch (error) {
    self.postMessage({ type: "error", message: error instanceof TypeError ? translate(locale, "cannotReachReleaseFiles") : error instanceof Error ? error.message : String(error) });
  } finally {
    busy = false;
  }
};
