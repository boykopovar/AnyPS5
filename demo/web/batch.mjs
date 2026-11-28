import { translate } from "./i18n.mjs";

export const MEMORY_LIMIT = 256 * 1024 * 1024;

function safeSize(value, locale) {
  if (!Number.isSafeInteger(value) || value < 0) throw new Error(translate(locale, "unsupportedFileSize"));
  return value;
}

export function memoryEstimate(archiveBytes, selectedBytes, libraryBytes, entries, locale = "en") {
  for (const value of [archiveBytes, selectedBytes, libraryBytes, entries]) safeSize(value, locale);
  return safeSize(Math.ceil(archiveBytes * 1.02) + selectedBytes * 4 + libraryBytes + entries * 1024 + 8 * 1024 * 1024, locale);
}

export function batchMemoryEstimate(summaries, target, libraryBytes, locale = "en") {
  if (target !== "linux" && target !== "windows") throw new Error(translate(locale, "unsupportedTarget", { target }));
  return summaries.reduce((total, summary) => safeSize(total + memoryEstimate(
    summary.totalBytes,
    target === "windows" ? summary.windowsSelectedBytes : summary.selectedBytes,
    libraryBytes,
    summary.files,
    locale
  ), locale), 0);
}

export function batchOutputDirectory(name, index, locale = "en") {
  if (!Number.isSafeInteger(index) || index < 0) throw new Error(translate(locale, "invalidAppIndex"));
  const stem = String(name).replace(/\.zip$/iu, "").normalize("NFKC")
    .replace(/[^A-Za-z0-9._-]+/gu, "-").replace(/^[.-]+/u, "").slice(0, 80).replace(/[. -]+$/u, "");
  return `${String(index + 1).padStart(2, "0")}-${stem || "app"}`;
}
