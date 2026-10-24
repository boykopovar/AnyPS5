import { abortAndDiscardOutput, OutputDirectoryTransaction } from "./output-directory.mjs";
import { batchMemoryEstimate } from "./batch.mjs";

const $ = id => document.getElementById(id);
const stages = ["Converting executable", "Converting PRX modules", "Scanning syscalls", "Scanning instructions", "Converting instructions", "Analysing imports", "Building output image", "Writing output"];
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
let logs = [];
let release;
const canSaveZip = typeof window.showSaveFilePicker === "function" && window.isSecureContext;
const canStream = typeof window.showDirectoryPicker === "function" && window.isSecureContext;
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

function update() {
  const target = document.querySelector('input[name="target"]:checked').value;
  const multi = selectedInputs.length > 1;
  const inspected = inspectionComplete && summaries.length === selectedInputs.length;
  const folderBlocked = $("save").value === "disk" && (target === "windows" || multi);
  const folderOption = $("save").querySelector('[value="disk"]');
  folderOption.disabled = !canStream || target === "windows" || multi;
  folderOption.textContent = target === "windows" ? "Save to folder · unavailable for Windows (DLL restrictions)" : multi ? "Save to folder · unavailable for batch output" : "Save to folder · uncompressed";
  const libraryBytes = release?.platforms[target]?.expandedBytes;
  const selectedBytes = inspected ? sum(summaries.map(summary => target === "windows" ? summary.windowsSelectedBytes : summary.selectedBytes)) : null;
  const conversionTooLarge = inspected && summaries.some(summary => (target === "windows" ? summary.windowsSelectedBytes : summary.selectedBytes) > 256 * 1024 * 1024);
  const iconTooLarge = target === "windows" && inspected && summaries.some(summary => summary.windowsIconTooLarge);
  const conversionBlocked = conversionTooLarge || iconTooLarge;
  let estimate = null;
  let estimateOverflow = false;
  if (inspected && libraryBytes !== undefined) {
    try { estimate = batchMemoryEstimate(summaries, target, libraryBytes); }
    catch (error) {
      if (!(error instanceof Error) || error.message !== "ZIP contains an unsupported file size") throw error;
      estimateOverflow = true;
    }
  }
  const tooLarge = estimateOverflow || (estimate !== null && estimate > 256 * 1024 * 1024);
  $("save").querySelector('[value="memory"]').disabled = Boolean(tooLarge);
  const memoryBlocked = $("save").value === "memory" && tooLarge;
  if (inspected) {
    const totalFiles = sum(summaries.map(summary => summary.files));
    const totalBytes = sum(summaries.map(summary => summary.totalBytes));
    const modules = sum(summaries.map(summary => summary.modules));
    const sizeText = value => value === null ? "beyond the supported size range" : bytes(value);
    $("archive-info").textContent = multi
      ? `${selectedInputs.length} apps · ${sizeText(totalFiles)} files · ${sizeText(totalBytes)} expanded · ${sizeText(modules)} modules · ${sizeText(selectedBytes)} ${target} conversion input`
      : `${summaries[0].files} files · ${bytes(summaries[0].totalBytes)} expanded · ${summaries[0].modules} modules · ${bytes(target === "windows" ? summaries[0].windowsSelectedBytes : summaries[0].selectedBytes)} ${target} conversion input` + (summaries[0].root ? ` · Root: ${summaries[0].root}` : "");
  }
  $("save-note").textContent = multi ? $("save").value === "memory" ? "Creates one ZIP with each app in its own folder. The complete bundle must fit in the 256 MiB memory-download budget." : "Writes one compressed ZIP containing each app in its own folder, without holding the whole bundle in memory." : $("save").value === "disk" ? "Creates a subfolder with the executable, libs and app0. No ZIP compression. Incomplete output folders are removed after cancellation or failure. Browsers may block DLL and other restricted files; use ZIP for those apps." : $("save").value === "zip" ? "Writes a compressed ZIP directly to disk without holding the whole archive in memory." : "Up to 256 MiB including expanded files and output overhead.";
  $("browser-note").hidden = supported && canStream && canSaveZip && !memoryBlocked && !folderBlocked && !conversionBlocked;
  $("browser-note").textContent = !supported ? "This browser or connection does not support the converter. Use a current browser over HTTPS, or use the native relinker." : folderBlocked && multi ? "Batch conversion creates one ZIP bundle. Choose Save ZIP to disk or Download in memory." : folderBlocked ? "Browsers block direct DLL creation. Windows output requires ZIP. Choose Save ZIP to disk or Download in memory." : iconTooLarge ? "A Windows icon exceeds 16 MiB. Choose Linux or use the native relinker." : conversionTooLarge ? `A ${target} conversion input exceeds 256 MiB. Use the native relinker for that app.` : estimateOverflow ? "The combined output exceeds the browser's supported size range. Choose Save ZIP to disk." : memoryBlocked ? "This output exceeds the memory-download limit. Choose Save ZIP to disk. If that option is unavailable, open this page in desktop Chrome or Edge." : "Some direct disk saving options are unavailable in this browser or context. Small apps can still be downloaded in memory. For larger apps, use desktop Chrome or Edge over HTTPS.";
  const hasWarnings = inspected && summaries.some(summary => summary.warnings.length > 0);
  $("convert").disabled = !supported || working || !inspected || !release || conversionBlocked || memoryBlocked || folderBlocked || (hasWarnings && !$("acknowledge").checked);
  $("ready-note").textContent = inspected ? (folderBlocked && multi ? "Choose a ZIP save mode for the app bundle." : folderBlocked ? "Choose a ZIP save mode for Windows." : iconTooLarge ? "A Windows icon exceeds 16 MiB. Choose Linux or use the native relinker." : conversionTooLarge ? `A ${target} conversion input exceeds 256 MiB. Use the native relinker for that app.` : memoryBlocked ? "A direct disk save is required for this output." : release ? `Libraries: ${release.tag} · ${multi ? "ZIP bundle" : $("save").value === "disk" ? "Uncompressed folder" : "ZIP compression level 9"}` : "Loading release information…") : "Choose one folder or one or more ZIP files to get started.";
}

function lock(value) {
  for (const id of ["choose", "choose-folder", "change", "target", "save", "intel"]) $(id).disabled = value || !supported || (id === "choose-folder" && !("webkitdirectory" in $("folder")));
}

async function fail(message) {
  if (failing) return;
  failing = true;
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
    $("phase").textContent = "Removing incomplete output folder";
  }
  const activeSink = sink;
  sink = null;
  for (const failure of await abortAndDiscardOutput(transaction, activeSink)) message += `\n${failure}`;
  $("error").textContent = message;
  $("error").hidden = false;
  $("status").hidden = true;
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
}

async function onMessage({ data }) {
  try {
    if (data.type === "progress") {
      if (data.message) {
        logs.push(`${data.appName ? `${data.appName}: ` : ""}${data.message.trim()}`);
        logs = logs.slice(-150);
        $("log").textContent = logs.join("\n");
        return;
      }
      const phase = data.phase ?? stages[data.stage];
      $("phase").textContent = data.appCount > 1 ? `App ${data.appIndex + 1}/${data.appCount} · ${phase}` : phase;
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
      inspectionComplete = true;
      working = false;
      $("selection").hidden = false;
      $("drop").hidden = true;
      $("archive-name").textContent = selectedInputs.length > 1 ? `${selectedInputs.length} app ZIPs` : selectedInputs[0].name;
      $("app-list").replaceChildren(...selectedInputs.map((input, index) => {
        const item = document.createElement("li");
        const name = document.createElement("strong");
        name.textContent = input.name;
        const details = document.createElement("span");
        const summary = summaries[index];
        details.textContent = `${summary.files} files · ${bytes(summary.totalBytes)} expanded · ${summary.modules} modules`;
        item.append(name, details);
        return item;
      }));
      $("app-list").hidden = selectedInputs.length < 2;
      const warningItems = [];
      for (let index = 0; index < summaries.length; index++) {
        for (const message of summaries[index].warnings) {
          const item = document.createElement("li");
          item.textContent = selectedInputs.length > 1 ? `${selectedInputs[index].name}: ${message}` : message;
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
      if (!outputDirectory) throw new Error("No output folder is open");
      const parts = data.name.replace(/\/$/u, "").split("/");
      if (parts.some(part => !part || part === "." || part === ".." || part.includes("\\"))) throw new Error("Invalid output path");
      const filename = data.type === "open-file" ? parts.pop() : null;
      let parent = outputDirectory;
      for (const part of parts) parent = await parent.getDirectoryHandle(part, { create: true });
      if (filename) {
        try {
          const handle = await parent.getFileHandle(filename, { create: true });
          sink = await handle.createWritable();
        } catch (error) {
          throw new Error(`Cannot write ${data.name}: ${error.message}. Choose ZIP output if the browser blocks this file type.`);
        }
      }
      worker?.postMessage({ type: "written" });
    } else if (data.type === "close-file") {
      if (!sink) throw new Error("No output file is open");
      await sink.close();
      sink = null;
      worker?.postMessage({ type: "written" });
    } else if (data.type === "write") {
      if (!sink) throw new Error("No output file is open");
      await sink.write(data.bytes);
      worker?.postMessage({ type: "written" });
    } else if (data.type === "complete") {
      if (sink) { await sink.close(); sink = null; }
      working = false;
      $("done").hidden = false;
      $("status").hidden = true;
      $("cancel").hidden = true;
      $("result-info").textContent = `${data.apps > 1 ? `${data.apps} apps · ` : ""}${bytes(data.size)} ${data.mode === "disk" ? "files" : "ZIP"} · ${data.release} system libraries · ${data.apps > 1 ? "combined app bundle" : data.outputName}`;
      if (data.blob) {
        downloadUrl = URL.createObjectURL(data.blob);
        $("download").href = downloadUrl;
        $("download").download = `${data.outputName}.zip`;
        $("download").hidden = false;
      } else if (data.mode === "zip") {
        $("result-info").textContent += " · ZIP saved to disk";
      } else {
        $("result-info").textContent += ` · Saved in ${outputTransaction.name}/`;
        if (document.querySelector('input[name="target"]:checked').value === "linux") $("result-info").textContent += ` · On Linux, run chmod +x "${data.outputName}" before launching.`;
      }
      $("result-note").textContent = data.apps > 1 ? "Extract the ZIP; each app is in its own folder with its executable, libs and app0 files. On Linux, run chmod +x */*.elf before launching." : "Extract the entire ZIP before launching the executable.";
      $("result-note").hidden = data.mode === "disk";
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
    } else if (data.type === "error") await fail(data.message);
  } catch (error) { await fail(error.message); }
}

async function selectInputs(inputs) {
  if (working || !supported || !inputs.length) return;
  if (inputs.some(input => !input.files && !input.name.toLowerCase().endsWith(".zip")) || (inputs.length > 1 && inputs.some(input => input.files))) {
    await fail("Batch conversion accepts ZIP archives only. Choose one folder or select ZIP files.");
    return;
  }
  worker?.terminate();
  if (downloadUrl) { URL.revokeObjectURL(downloadUrl); downloadUrl = null; }
  selectedInputs = inputs;
  summaries = [];
  inspectionComplete = false;
  working = true;
  logs = [];
  $("log").textContent = "";
  for (const id of ["error", "done", "download", "warnings", "selection"]) $(id).hidden = true;
  $("acknowledge").checked = false;
  $("status").hidden = false;
  $("cancel").hidden = false;
  $("phase").textContent = "Inspecting files";
  $("current-file").textContent = inputs.length > 1 ? `Inspecting ${inputs.length} ZIP archives` : inputs[0].name;
  $("percent").textContent = "";
  $("progress").removeAttribute("value");
  lock(true);
  update();
  worker = new Worker(new URL("./worker.mjs", import.meta.url), { type: "module" });
  worker.onmessage = onMessage;
  worker.onerror = event => fail(event.message || "The converter worker stopped. Try a smaller app or use the native relinker.");
  worker.postMessage({ type: "inspect", inputs });
}

$("choose").onclick = () => { $("file").value = ""; $("file").click(); };
$("change").onclick = () => {
  if (working) return;
  if (downloadUrl) { URL.revokeObjectURL(downloadUrl); downloadUrl = null; }
  file = null;
  summary = null;
  $("file").value = "";
  $("folder").value = "";
  for (const id of ["selection", "warnings", "error", "done", "download"]) $(id).hidden = true;
  $("acknowledge").checked = false;
  $("drop").hidden = false;
  update();
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
$("cancel").onclick = () => fail("Cancelled. Choose the ZIP or folder again to restart.");
$("convert").onclick = async () => {
  if ($("convert").disabled) return;
  working = true;
  lock(true);
  update();
  try {
    const target = document.querySelector('input[name="target"]:checked').value;
    if ($("save").value === "zip") {
      const name = selectedInputs.length > 1 ? `anyps5-${target}-apps.zip` : `${selectedInputs[0].name.replace(/\.zip$/iu, "")}-${target}.zip`;
      const handle = await window.showSaveFilePicker({ suggestedName: name, types: [{ description: "ZIP archive", accept: { "application/zip": [".zip"] } }] });
      sink = await handle.createWritable();
    } else if ($("save").value === "disk") {
      const parent = await window.showDirectoryPicker({ mode: "readwrite" });
      const name = selectedInputs[0].name.replace(/\.zip$/iu, "") + "-" + target;
      outputTransaction = await OutputDirectoryTransaction.create(parent, name);
      outputDirectory = outputTransaction.directory;
    }
    $("status").hidden = false;
    $("cancel").hidden = false;
    $("error").hidden = true;
    worker.postMessage({ type: "convert", target, toIntel: $("intel").checked, mode: $("save").value });
  } catch (error) {
    if (error.name === "AbortError") { working = false; lock(false); update(); }
    else await fail(error.message);
  }
};
$("theme").onclick = () => {
  const dark = document.documentElement.dataset.theme ? document.documentElement.dataset.theme === "dark" : matchMedia("(prefers-color-scheme: dark)").matches;
  document.documentElement.dataset.theme = dark ? "light" : "dark";
};
update();
lock(false);
if (supported) fetch("./release.json", { cache: "no-cache" }).then(response => { if (!response.ok) throw new Error(`Cannot load release information (${response.status}).${response.status === 404 ? " release.json is generated during the site build, so it is missing when serving demo/web directly." : ""}`); return response.json(); }).then(value => {
  if (!value.tag || !["windows", "linux"].every(target => Number.isSafeInteger(value.platforms?.[target]?.expandedBytes))) throw new Error("Invalid release information");
  release = value;
  update();
}).catch(error => fail(error.message));

if (!("webkitdirectory" in $("folder"))) {
  $("choose-folder").disabled = true;
  $("choose-folder").title = "Folder selection is unavailable in this browser. Choose a ZIP instead.";
}
