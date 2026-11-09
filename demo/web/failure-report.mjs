const LOG_TAIL = 150;

export function failureReport({ message, phase, input, target, release, log }) {
  const context = [];
  if (phase) context.push(`Phase: ${phase}`);
  if (input) context.push(`Input: ${input}`);
  if (target) context.push(`Target: ${target}`);
  if (release?.tag) context.push(`Libraries: ${release.tag}`);
  const tail = (log ?? []).slice(-LOG_TAIL);
  const sections = [String(message)];
  if (context.length) sections.push(context.join("\n"));
  if (tail.length) sections.push(`Conversion log (last ${tail.length} lines):\n${tail.join("\n")}`);
  return sections.join("\n\n");
}
