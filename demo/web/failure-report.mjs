import { translate } from "./i18n.mjs";

const LOG_TAIL = 150;

export function failureReport({ message, phase, input, target, release, log, locale = "en" }) {
  const context = [];
  if (phase) context.push(translate(locale, "reportPhase", { phase }));
  if (input) context.push(translate(locale, "reportInput", { input }));
  if (target) context.push(translate(locale, "reportTarget", { target }));
  if (release?.tag) context.push(translate(locale, "reportLibraries", { tag: release.tag }));
  const tail = (log ?? []).slice(-LOG_TAIL);
  const sections = [String(message)];
  if (context.length) sections.push(context.join("\n"));
  if (tail.length) sections.push(translate(locale, "reportLog", { count: tail.length, log: tail.join("\n") }));
  return sections.join("\n\n");
}
