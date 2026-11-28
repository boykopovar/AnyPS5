import { abortAndDiscardOutput, OutputDirectoryTransaction } from "./output-directory.mjs";
import { batchMemoryEstimate, MEMORY_LIMIT, memoryEstimate } from "./batch.mjs";
import { failureReport } from "./failure-report.mjs";
import { apply, current, locales, set as setLocale, translate } from "./i18n.mjs";

const $ = id => document.getElementById(id);
let locale = current();
apply(locale);
let releaseRequest;
let releaseVersion = 0;
let worker;
let selectedInputs = [];
let summaries = [];
let inspectionComplete = false;
let working = false;
let failing = false;
let sink;
let outputDirectory;
let outputTransaction;
let downloadUrl;
let completedResult;
let logs = [];
let release;
let progressState = null;
const canSaveZip = typeof window.showSaveFilePicker === "function" && window.isSecureContext;
const folderLocks = window.navigator?.locks;
const canStream = typeof window.showDirectoryPicker === "function" && typeof folderLocks?.request === "function" && window.isSecureContext;
const supported = window.isSecureContext && window.Worker && window.WebAssembly && window.ReadableStream && window.WritableStream && window.crypto?.subtle;
$("save").querySelector('[value="disk"]').disabled = !canStream;
$("save").querySelector('[value="zip"]').disabled = !canSaveZip;
$("save").value = canSaveZip ? "zip" : "memory";

function bytes(value) {
  if (value < 1024) return `${value} B`;
  const unit = value >= 1024 ** 3 ? 3 : value >= 1024 ** 2 ? 2 : 1;
  return `${(value / 1024 ** unit).toFixed(1)} ${["B", "KiB", "MiB", "GiB"][unit]}`;
}

function sum(values) {
  let total = 0;
  for (const value of values) {
    if (!Number.isSafeInteger(value) || value < 0 || total > Number.MAX_SAFE_INTEGER - value) return null;
    total += value;
  }
  return total;
}

function updateLocaleButton() {
  $("locale").textContent = locale.toUpperCase();
}

function renderSelection() {
  const multi = selectedInputs.length > 1;
  $("archive-name").textContent = multi ? translate(locale, "batchArchiveName", { count: selectedInputs.length }) : selectedInputs[0].name;
  $("app-list").replaceChildren(...selectedInputs.map((input, index) => {
    const item = document.createElement("li");
    const name = document.createElement("strong");
    name.textContent = input.name;
    const details = document.createElement("span");
    const summary = summaries[index];
    details.textContent = translate(locale, "appListDetails", { files: summary.files, size: bytes(summary.totalBytes), modules: summary.modules });
    item.append(name, details);
    return item;
  }));
  $("app-list").hidden = !multi;
}

function update() {
  const target = document.querySelector('input[name="target"]:checked').value;
  const multi = selectedInputs.length > 1;
  const inspected = inspectionComplete && summaries.length === selectedInputs.length;
  $("patch-options").hidden = !inspected || multi;
  $("patch").disabled = working || !inspected || multi;
  const folderBlocked = $("save").value === "disk" && (target === "windows" || multi);
  const folderOption = $("save").querySelector('[value="disk"]');
  folderOption.disabled = !canStream || target === "windows" || multi;
  folderOption.textContent = target === "windows" ? translate(locale, "saveFolderWindowsBlocked") : multi ? translate(locale, "saveFolderBatchBlocked") : translate(locale, "saveOptionDisk");
  const libraryBytes = release?.platforms[target]?.expandedBytes;
  const selectedBytes = inspected ? sum(summaries.map(summary => target === "windows" ? summary.windowsSelectedBytes : summary.selectedBytes)) : null;
  const conversionTooLarge = inspected && summaries.some(summary => (target === "windows" ? summary.windowsSelectedBytes : summary.selectedBytes) > 256 * 1024 * 1024);
  const iconTooLarge = target === "windows" && inspected && summaries.some(summary => summary.windowsIconTooLarge);
  const conversionBlocked = conversionTooLarge || iconTooLarge;
  let estimate = null;
  let estimateOverflow = false;
  if (libraryBytes !== undefined) {
    try {
      estimate = inspected
        ? batchMemoryEstimate(summaries, target, libraryBytes, locale)
        : memoryEstimate(0, 0, libraryBytes, 0, locale);
    }
    catch (error) {
      if (!(error instanceof Error) || error.message !== translate(locale, "unsupportedFileSize")) throw error;
      estimateOverflow = true;
    }
  }
  const tooLarge = estimateOverflow || (estimate !== null && estimate > MEMORY_LIMIT);
  $("save").querySelector('[value="memory"]').disabled = Boolean(tooLarge);
  const memoryBlocked = $("save").value === "memory" && tooLarge;
  if (inspected) {
    const totalFiles = sum(summaries.map(summary => summary.files));
    const totalBytes = sum(summaries.map(summary => summary.totalBytes));
    const modules = sum(summaries.map(summary => summary.modules));
    const sizeText = value => value === null ? translate(locale, "sizeOutOfRange") : bytes(value);
    const countText = value => value === null ? translate(locale, "countOutOfRange") : value;
    $("archive-info").textContent = multi
      ? translate(locale, "archiveInfoBatch", { count: selectedInputs.length, files: countText(totalFiles), size: sizeText(totalBytes), modules: countText(modules), selected: sizeText(selectedBytes), target })
      : translate(locale, "archiveInfoSingle", { files: summaries[0].files, size: bytes(summaries[0].totalBytes), modules: summaries[0].modules, selected: bytes(target === "windows" ? summaries[0].windowsSelectedBytes : summaries[0].selectedBytes), target })
        + (summaries[0].root ? translate(locale, "archiveInfoRoot", { root: summaries[0].root }) : "");
  }
  $("save-note").textContent = multi
    ? $("save").value === "memory" ? translate(locale, "saveNoteBatchMemory") : translate(locale, "saveNoteBatchZip")
    : $("save").value === "disk" ? translate(locale, "saveNoteDisk") : $("save").value === "zip" ? translate(locale, "saveNoteZip") : translate(locale, "saveNoteDefault");
  $("browser-note").hidden = supported && canStream && canSaveZip && !memoryBlocked && !folderBlocked && !conversionBlocked;
  $("browser-note").textContent = !supported ? translate(locale, "browserUnsupported")
    : folderBlocked && multi ? translate(locale, "browserFolderBlockedBatch")
    : folderBlocked ? translate(locale, "browserFolderBlockedWindows")
    : iconTooLarge ? translate(locale, "iconTooLargeHint")
    : conversionTooLarge ? translate(locale, "conversionTooLargeHint", { target })
    : estimateOverflow ? translate(locale, "estimateOverflow")
    : tooLarge ? translate(locale, "memoryBlockedHint")
    : translate(locale, "browserDefault");
  const hasWarnings = inspected && summaries.some(summary => summary.warnings.length > 0);
  $("convert").disabled = !supported || working || !inspected || !release || conversionBlocked || memoryBlocked || folderBlocked || (hasWarnings && !$("acknowledge").checked);
  $("ready-note").textContent = inspected
    ? (folderBlocked && multi ? translate(locale, "readyFolderBlockedBatch")
      : folderBlocked ? translate(locale, "readyFolderBlockedWindows")
      : iconTooLarge ? translate(locale, "iconTooLargeHint")
      : conversionTooLarge ? translate(locale, "conversionTooLargeHint", { target })
      : memoryBlocked ? translate(locale, "readyMemoryRequired")
      : release ? translate(locale, "readyLibraries", { tag: release.tag, mode: multi ? translate(locale, "readyModeBatch") : $("save").value === "disk" ? translate(locale, "readyModeDisk") : translate(locale, "readyModeZip") })
      : translate(locale, "readyLoading"))
    : translate(locale, "readyChooseInput");
  renderProgress();
  renderError();
}

function renderProgress() {
  if (!progressState) return;
  const { phaseKey, phase, stage, appCount, appIndex, fileKey, fileValues } = progressState;
  const text = phaseKey ? translate(locale, phaseKey) : phase ?? translate(locale, `stage${stage}`);
  $("phase").textContent = appCount > 1 ? translate(locale, "appProgress", { index: appIndex + 1, count: appCount, phase: text }) : text;
  if (fileKey) $("current-file").textContent = translate(locale, fileKey, fileValues);
}

function lock(value) {
  for (const id of ["choose", "choose-folder", "change", "target", "save", "intel"]) $(id).disabled = value || !supported || (id === "choose-folder" && !("webkitdirectory" in $("folder")));
}

let errorState = null;

function messageText(value) {
  if (typeof value === "function") return value();
  if (value instanceof Error) return value.localized ? value.localized() : value.message;
  return value;
}

function localizedError(render) {
  const error = new Error(render());
  Object.defineProperty(error, "localized", { value: render, enumerable: false });
  return error;
}

function renderError() {
  const error = $("error");
  if (!errorState || error.hidden || errorState.renderedLocale === locale) return;
  const message = [messageText(errorState.message), ...errorState.appended].join("\n");
  const report = failureReport({ ...errorState.context, message, locale });
  if (!errorState.view) {
    const headline = document.createTextNode(message);
    const view = { headline };
    if (report === message) error.replaceChildren(headline);
    else {
      const details = document.createElement("details");
      view.summary = document.createElement("summary");
      view.text = document.createElement("pre");
      details.append(view.summary, view.text);
      error.replaceChildren(headline, details);
    }
    errorState.view = view;
  }
  const { headline, summary, text } = errorState.view;
  headline.textContent = message;
  if (summary) {
    summary.textContent = translate(locale, "failureDetails");
    text.textContent = report;
  }
  errorState.renderedLocale = locale;
}

async function fail(value, { keepInspection = false } = {}) {
  if (failing) return;
  failing = true;
  progressState = null;
  errorState = {
    message: value,
    appended: [],
    context: {
      phase: $("phase").textContent,
      input: $("current-file").textContent,
      target: document.querySelector('input[name="target"]:checked').value,
      release,
      log: logs
    }
  };
  if (keepInspection) {
    $("error").hidden = false;
    $("cancel").hidden = true;
    working = false;
    failing = false;
    lock(false);
    update();
    return;
  }
  releaseVersion++;
  releaseRequest = null;
  worker?.terminate();
  worker = null;
  working = true;
  selectedInputs = [];
  summaries = [];
  inspectionComplete = false;
  const transaction = outputTransaction;
  outputTransaction = null;
  outputDirectory = null;
  if (transaction) {
    $("status").hidden = false;
    progressState = { phaseKey: "removingOutputFolder" };
    renderProgress();
  }
  const activeSink = sink;
  sink = null;
  errorState.appended = await abortAndDiscardOutput(transaction, activeSink, locale);
  progressState = null;
  $("error").hidden = false;
  $("status").hidden = true;
  const restoreFocus = document.activeElement === $("cancel");
  $("cancel").hidden = true;
  $("drop").hidden = false;
  $("selection").hidden = true;
  $("warnings").hidden = true;
  $("app-list").replaceChildren();
  $("file").value = "";
  working = false;
  failing = false;
  lock(false);
  update();
  if (restoreFocus) $("choose").focus();
}

async function onMessage({ data }, currentWorker) {
  if (currentWorker !== worker || failing) return;
  try {
    if (data.type === "progress") {
      if (data.message) {
        const entry = `${data.appName ? `${data.appName}: ` : ""}${data.message.trim()}`;
        logs = logs.concat(entry.split(/\r\n|[\r\n]/u)).slice(-150);
        $("log").textContent = logs.join("\n");
        return;
      }
      progressState = data;
      renderProgress();
      $("current-file").textContent = [data.appName, data.input].filter(Boolean).join(" · ");
      if (Number.isFinite(data.total) && data.total > 0) {
        const percent = Math.min(100, Math.floor(data.completed / data.total * 100));
        $("progress").value = percent;
        $("percent").textContent = `${percent}%`;
      } else {
        $("progress").removeAttribute("value");
        $("percent").textContent = "";
      }
    } else if (data.type === "inspected") {
      summaries[data.index] = data.summary;
    } else if (data.type === "inspection-complete") {
      progressState = null;
      inspectionComplete = true;
      working = false;
      $("selection").hidden = false;
      $("drop").hidden = true;
      renderSelection();
      const warningItems = [];
      for (let index = 0; index < summaries.length; index++) {
        for (const message of summaries[index].warnings) {
          const item = document.createElement("li");
          item.textContent = selectedInputs.length > 1 ? translate(locale, "warningPrefixed", { name: selectedInputs[index].name, message }) : message;
          warningItems.push(item);
        }
      }
      $("warning-list").replaceChildren(...warningItems);
      $("warnings").hidden = warningItems.length === 0;
      $("status").hidden = true;
      $("cancel").hidden = true;
      lock(false);
      update();
    } else if (data.type === "directory" || data.type === "open-file") {
      if (!outputDirectory) throw localizedError(() => translate(locale, "noOutputFolderOpen"));
      const parts = data.name.replace(/\/$/u, "").split("/");
      if (parts.some(part => !part || part === "." || part === ".." || part.includes("\\"))) throw localizedError(() => translate(locale, "invalidOutputPath"));
      const filename = data.type === "open-file" ? parts.pop() : null;
      let parent = outputDirectory;
      for (const part of parts) {
        parent = await parent.getDirectoryHandle(part, { create: true });
        if (currentWorker !== worker) return;
      }
      if (filename) {
        try {
          const handle = await parent.getFileHandle(filename, { create: true });
          if (currentWorker !== worker) return;
          const writable = await handle.createWritable();
          if (currentWorker !== worker) {
            await writable.abort().catch(() => {});
            return;
          }
          sink = writable;
        } catch (error) {
          throw localizedError(() => translate(locale, "cannotWriteFile", { name: data.name, message: error.message }));
        }
      }
      if (currentWorker === worker) currentWorker.postMessage({ type: "written" });
    } else if (data.type === "close-file") {
      if (!sink) throw localizedError(() => translate(locale, "noOutputFileOpen"));
      await sink.close();
      if (currentWorker !== worker) return;
      sink = null;
      currentWorker.postMessage({ type: "written" });
    } else if (data.type === "write") {
      if (!sink) throw localizedError(() => translate(locale, "noOutputFileOpen"));
      await sink.write(data.bytes);
      if (currentWorker === worker) currentWorker.postMessage({ type: "written" });
    } else if (data.type === "complete") {
      const focusDownload = data.blob && document.activeElement === $("cancel");
      $("cancel").hidden = true;
      if (sink) {
        await sink.close();
        if (currentWorker !== worker) return;
        sink = null;
      }
      working = false;
      $("done").hidden = false;
      $("status").hidden = true;
      const { apps, mode, size, release: tag, revision, outputName } = data;
      completedResult = {
        apps, mode, size, tag, revision, outputName,
        folderName: mode === "disk" ? outputTransaction.name : null,
        target: document.querySelector('input[name="target"]:checked').value
      };
      renderResult();
      if (data.blob) {
        downloadUrl = URL.createObjectURL(data.blob);
        $("download").href = downloadUrl;
        $("download").download = outputName + ".zip";
        $("download").hidden = false;
      }
      outputDirectory = null;
      outputTransaction?.commit();
      outputTransaction = null;
      selectedInputs = [];
      summaries = [];
      inspectionComplete = false;
      worker.terminate();
      worker = null;
      lock(false);
      update();
      if (focusDownload) $("download").focus();
    } else if (data.type === "error") await fail(data.message);
  } catch (error) {
    if (currentWorker === worker) await fail(error);
  }
}

function renderResult() {
  if (!completedResult || $("done").hidden) return;
  const { apps, mode, size, tag, revision, outputName, folderName, target } = completedResult;
  const unit = mode === "disk" ? translate(locale, "resultUnitFiles") : "ZIP";
  const values = { apps, size: bytes(size), unit, release: revision ? tag + " (" + revision + ")" : tag, outputName };
  let info = translate(locale, apps > 1 ? "resultInfoBatch" : "resultInfoSingle", values);
  if (mode === "zip") {
    info += translate(locale, "resultZipSavedToDisk");
  } else if (mode === "disk") {
    info += translate(locale, "resultSavedIn", { name: folderName });
    if (target === "linux") info += translate(locale, "resultLinuxChmod", { name: outputName });
  }
  $("result-info").textContent = info;
  $("result-note").textContent = translate(locale, apps > 1 ? "resultNoteBatch" : "resultNoteSingle");
  $("result-note").hidden = mode === "disk";
}

async function selectInputs(inputs) {
  if (working || !supported || !inputs.length) return;
  if (inputs.some(input => !input.files && !input.name.toLowerCase().endsWith(".zip")) || (inputs.length > 1 && inputs.some(input => input.files))) {
    await fail(() => translate(locale, "batchZipOnly"));
    return;
  }
  worker?.terminate();
  if (downloadUrl) { URL.revokeObjectURL(downloadUrl); downloadUrl = null; }
  selectedInputs = inputs;
  $("patch").value = "";
  summaries = [];
  inspectionComplete = false;
  working = true;
  progressState = inputs.length > 1 ? { phaseKey: "inspectingFiles", fileKey: "inspectingBatch", fileValues: { count: inputs.length } } : { phaseKey: "inspectingFiles" };
  logs = [];
  $("log").textContent = "";
  for (const id of ["error", "done", "download", "warnings", "selection"]) $(id).hidden = true;
  $("acknowledge").checked = false;
  $("status").hidden = false;
  $("cancel").hidden = false;
  $("phase").textContent = translate(locale, "inspectingFiles");
  $("current-file").textContent = inputs.length > 1 ? translate(locale, "inspectingBatch", { count: inputs.length }) : inputs[0].name;
  $("percent").textContent = "";
  $("progress").removeAttribute("value");
  lock(true);
  update();
  try {
    const currentWorker = new Worker(new URL("./worker.mjs", import.meta.url), { type: "module" });
    worker = currentWorker;
    currentWorker.onmessage = event => onMessage(event, currentWorker);
    currentWorker.onerror = event => {
      if (currentWorker === worker) return fail(event.message || (() => translate(locale, "workerStopped")));
    };
    currentWorker.postMessage({ type: "inspect", inputs, locale });
    loadReleaseInfo();
  } catch (error) {
    await fail(error.message);
  }
}

$("choose").onclick = () => { $("file").value = ""; $("file").click(); };
$("change").onclick = () => {
  if (working) return;
  const restoreFocus = document.activeElement === $("change");
  worker?.terminate();
  worker = null;
  if (downloadUrl) { URL.revokeObjectURL(downloadUrl); downloadUrl = null; }
  selectedInputs = [];
  summaries = [];
  inspectionComplete = false;
  $("file").value = "";
  $("folder").value = "";
  $("patch").value = "";
  $("app-list").replaceChildren();
  for (const id of ["selection", "warnings", "error", "done", "download", "app-list"]) $(id).hidden = true;
  $("acknowledge").checked = false;
  $("drop").hidden = false;
  update();
  if (restoreFocus) $("choose").focus();
};
$("choose-folder").onclick = () => { $("folder").value = ""; $("folder").click(); };
$("folder").onchange = () => {
  const files = [...$("folder").files].map(file => ({ path: file.webkitRelativePath, file }));
  if (files.length) selectInputs([{ name: files[0].path.split("/")[0], files }]);
};
$("file").onchange = () => {
  const inputs = [...$("file").files].map(file => ({ name: file.name, file }));
  $("file").value = "";
  selectInputs(inputs);
};
for (const type of ["dragenter", "dragover"]) $("drop").addEventListener(type, event => { event.preventDefault(); if (!working) $("drop").classList.add("dragging"); });
$("drop").ondragleave = () => $("drop").classList.remove("dragging");
$("drop").ondrop = event => {
  event.preventDefault();
  $("drop").classList.remove("dragging");
  if (working) return;
  selectInputs([...event.dataTransfer.files].map(file => ({ name: file.name, file })));
};
for (const id of ["save", "target", "acknowledge"]) $(id).addEventListener("change", update);
$("cancel").onclick = () => {
  if (!$("cancel").hidden) return fail(() => translate(locale, "cancelled"));
};
async function createZipSink(name, current) {
  try {
    const handle = await window.showSaveFilePicker({ suggestedName: name, types: [{ description: "ZIP archive", accept: { "application/zip": [".zip"] } }] });
    if (!current()) return null;
    const writable = await handle.createWritable();
    if (!current()) {
      await writable.abort().catch(() => {});
      return null;
    }
    return writable;
  } catch (error) {
    if (error.name === "AbortError") throw error;
    if (error.name === "NotAllowedError" || error.name === "SecurityError") throw localizedError(() => translate(locale, "zipDirectDenied"));
    throw error;
  }
}
$("convert").onclick = async () => {
  if ($("convert").disabled) return;
  const currentWorker = worker;
  let preparingOutput = true;
  working = true;
  $("error").hidden = true;
  lock(true);
  update();
  try {
    const target = document.querySelector('input[name="target"]:checked').value;
    if ($("save").value === "zip") {
      $("cancel").hidden = false;
      const name = selectedInputs.length > 1 ? `anyps5-${target}-apps.zip` : `${selectedInputs[0].name.replace(/\.zip$/iu, "")}-${target}.zip`;
      const writable = await createZipSink(name, () => currentWorker === worker);
      if (writable === null) return;
      sink = writable;
    } else if ($("save").value === "disk") {
      $("cancel").hidden = false;
      const name = selectedInputs[0].name.replace(/\.zip$/iu, "") + "-" + target;
      const parent = await window.showDirectoryPicker({ mode: "readwrite" });
      if (currentWorker !== worker) return;
      const transaction = await OutputDirectoryTransaction.create(parent, name, folderLocks, locale, () => currentWorker === worker);
      if (!transaction) return;
      if (currentWorker !== worker) {
        await transaction.discard();
        return;
      }
      outputTransaction = transaction;
      outputDirectory = transaction.directory;
    }
    preparingOutput = false;
    $("status").hidden = false;
    $("cancel").hidden = false;
    $("error").hidden = true;
    const patchFile = selectedInputs.length === 1 ? $("patch").files?.[0] : undefined;
    currentWorker.postMessage({ type: "convert", target, toIntel: $("intel").checked, mode: $("save").value, patchFile, locale });
  } catch (error) {
    if (currentWorker === worker && preparingOutput && error.name !== "AbortError") {
      await fail(error, { keepInspection: true });
      return;
    }
    if (currentWorker !== worker) return;
    if (error.name === "AbortError") {
      $("cancel").hidden = true;
      working = false;
      lock(false);
      update();
    } else await fail(error);
  }
};
$("theme").onclick = () => {
  const dark = document.documentElement.dataset.theme ? document.documentElement.dataset.theme === "dark" : matchMedia("(prefers-color-scheme: dark)").matches;
  document.documentElement.dataset.theme = dark ? "light" : "dark";
};
$("locale").onclick = () => {
  const index = locales.findIndex(item => item.code === locale);
  locale = locales[(index + 1) % locales.length].code;
  setLocale(locale);
  updateLocaleButton();
  if (inspectionComplete && selectedInputs.length > 1) renderSelection();
  renderResult();
  update();
};
updateLocaleButton();
update();
lock(false);
function loadReleaseInfo() {
  if (releaseRequest) return;
  release = undefined;
  const version = ++releaseVersion;
  releaseRequest = fetch("./release.json", { cache: "no-cache" }).then(response => { if (!response.ok) throw localizedError(() => translate(locale, "cannotLoadReleaseInfo", { status: response.status, hint: response.status === 404 ? translate(locale, "cannotLoadReleaseInfoHint") : "" })); return response.json(); }).then(value => {
    if (version !== releaseVersion) return;
    if (!value || typeof value !== "object" || typeof value.tag !== "string" || !/^v[0-9A-Za-z][0-9A-Za-z._-]*$/u.test(value.tag) || value.tag.endsWith(".") || !["windows", "linux"].every(target => {
      const size = value.platforms?.[target]?.expandedBytes;
      return Number.isSafeInteger(size) && size >= 0 && size <= 512 * 1024 * 1024;
    })) throw localizedError(() => translate(locale, "invalidReleaseInfo"));
    release = value;
    update();
  }).catch(error => {
    if (version === releaseVersion) return fail(error instanceof TypeError ? () => translate(locale, "cannotReachReleaseInfo") : error);
  }).finally(() => { if (version === releaseVersion) releaseRequest = null; });
}

if (supported) loadReleaseInfo();

if (!("webkitdirectory" in $("folder"))) {
  $("choose-folder").disabled = true;
  $("choose-folder").title = translate(locale, "folderUnsupported");
}
