export const MEMORY_LIMIT = 256 * 1024 * 1024;

function safeSize(value) {
  if (!Number.isSafeInteger(value) || value < 0) throw new Error("ZIP contains an unsupported file size");
  return value;
}

export function memoryEstimate(archiveBytes, selectedBytes, libraryBytes, entries) {
  for (const value of [archiveBytes, selectedBytes, libraryBytes, entries]) safeSize(value);
  return safeSize(Math.ceil(archiveBytes * 1.02) + selectedBytes * 4 + libraryBytes + entries * 1024 + 8 * 1024 * 1024);
}

export function batchMemoryEstimate(summaries, target, libraryBytes) {
  if (target !== "linux" && target !== "windows") throw new Error(`Unsupported conversion target: ${target}`);
  return summaries.reduce((total, summary) => safeSize(total + memoryEstimate(
    summary.totalBytes,
    target === "windows" ? summary.windowsSelectedBytes : summary.selectedBytes,
    libraryBytes,
    summary.files
  )), 0);
}

export function batchOutputDirectory(name, index) {
  if (!Number.isSafeInteger(index) || index < 0) throw new Error("Invalid app index");
  const stem = String(name).replace(/\.zip$/iu, "").normalize("NFKC")
    .replace(/[^A-Za-z0-9._-]+/gu, "-").replace(/^[.-]+/u, "").slice(0, 80).replace(/[. -]+$/u, "");
  return `${String(index + 1).padStart(2, "0")}-${stem || "app"}`;
}
