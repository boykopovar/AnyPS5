# Run report

`anyps5-report.json` is a structured summary of one run. [`run-report.schema.json`](../run-report.schema.json) is the single source of truth for its shape; the sample reports in [`run-report-fixtures/`](../run-report-fixtures/) all validate against it. This is version 1. Adding a field keeps version 1; renaming or removing a field, or changing what one means, is version 2.

The file stays on your machine. Nothing is uploaded unless you share it.

## Switching it off

The report is written by default. Turn it off with the launch flag `--aps5-report=off` or the environment variable `APS5_REPORT=0`. The flag wins over the environment variable, and the environment variable wins over the default (on). There is no ini setting. The flag needs a game converted with a new enough relinker; the environment variable also works with older conversions. The runtime reads the flag from the command line and does not remove it, so the game can see `--aps5-report=...` in its arguments. When the game relaunches itself, the choice is passed on through `APS5_REPORT`.

## Where it goes

`anyps5-report.json` is written next to the game's executable. If that folder can't be written, it goes to the AnyPS5 data folder under `reports/<run id>/` and the runtime prints where. `run.id` is that same id: 32 lowercase hex characters, random per run.

The report is written once, when the run ends through the guest's `exit` or `_exit`, and on Linux and macOS also when the entry returns or the host calls `std::exit`. On Windows only `exit` and `_exit` write a report: a return from the entry writes none, because exit handlers of a DLL run inside the loader lock. The Windows and macOS code and the junction test have not been run. A run that is killed, crashes or aborts writes no report.

## What `anyps5.modified` means

`false` means the build's git tree had no change to tracked files, no untracked files that git does not ignore, and no change inside a submodule. `true` means any of those. `null` means the build did not come from a git checkout. A mixed set of libraries from different builds is not detected.

## Unknown is null

A value the runtime cannot know is `null`, never a guess. This applies to `anyps5.*`, every `game.*` and `system.*` field, the whole `system.gpu` object `run.exitCode`, `stop.source`, and instruction details. Read null as "unknown", not as false or zero.

Every text value in the report goes through the same filter for user names, host names, addresses and account numbers. A version made of four numbers separated by dots (`1.2.3.4`) looks like an IPv4 address, so it is left out (null) rather than risk writing an address; versions of three numbers (`01.002.000`) and release tags (`v0.1.3`) are kept. The commit and the run id are fixed-length hex and are checked by their pattern only, so the build identity is always there.

`game.titleId` is copied only when it matches `PPSA` or `CUSA` style (four capital letters and five digits); otherwise it is null.

## Run

| Field | Meaning |
|---|---|
| `stage` | `unknown`: this build does not record milestones (`coverage.milestones` is false). |
| `exit` | `clean` for a run that ended through the exit path. |
| `exitCode` | The process exit code when known, otherwise null. |
| `framesPresented` | 0: this build does not count frames. |
| `recovered`, `incomplete` | `incomplete` is true when an observation had to be dropped. |

## Stop

`stop.kind` is `none` and `stop.reason` is `guest-exit` for a run that ended through the exit path. The schema lists the other kinds and reasons; this build writes only these.

## What `system` holds

`system.osVersion` is the operating system name and version (on Linux the distribution's `NAME` and `VERSION_ID` from `os-release` and the first three numbers of the kernel release, for example `Ubuntu 24.04 (kernel 5.15.153)`; whatever the vendor or the user appended to the kernel release is left out), `system.cpu` is the CPU model name (without a trailing clock speed such as ` @ 3.50GHz`), `system.ramGiB` is the installed memory in whole GiB, and `system.gpu` is the graphics device the runtime selected. `gpu` is null in this build. These are meant to be model and version names, and they are filtered like every other text: a string that contains your login, your home folder's name, an address, an account number, a UUID, a path, an e-mail address or a serial-number-like token is left out of the report (that field becomes null). Your host name is compared too, with one exception: in `system.osVersion` only, a host name is allowed when it equals the identifier of a public distribution from a fixed list in the source (`ubuntu`, `debian`, `fedora`, `arch`, `cachyos`, `manjaro`, `nixos` and so on) and the `ID` of the machine's `os-release` is that same entry (a machine called `cachyos` running CachyOS keeps `CachyOS`). The `ID` is text its owner can edit, so it counts only when it parses (matching quotes or none) and is on the list; `NAME` and `ID_LIKE` never count. A host named after a distribution that is not on the list, or whose `os-release` says something else, loses the OS name from `system.osVersion`. An `os-release` value with an unmatched quote is not used, and the OS version then falls back to what is left (for example `Linux (kernel 6.8.0)`). In `cpu`, `gpu.name` and `gpu.driver` every name is compared. A login is never given that allowance, so a login that equals a word of the distribution's name (`arch` on Arch Linux) makes `system.osVersion` null. Any machine string that contains your login is cleared as a whole, so a login `arc` also clears `Intel Arc`. The serial-like rule (twelve or more letters and digits with both) also drops a driver string that carries a long git hash, as development builds of Mesa do.

## What the runtime counts

- `missing.functions` counts calls to a function the runtime does not implement. The library is the folder the stub is compiled in; a call from older code that does not say where it is made has a null library. The name is the exported name without the runtime's suffix; a name that does not fit the schema is not listed and is counted in `truncated.functions`.
- `truncated` counts entries that were dropped, once each however many calls they made. When a hook had to give up an observation because the collector was busy, the matching count is at least 1 and the report is marked `incomplete`.
- Milestones, frame counts, shader counts and the shader and image lists are not collected by this build: their `coverage` flags are false.
- Nothing in this list is collected, counted or scanned when the report is off.

## Lists, coverage and truncation

- `missing.functions[]`: `library` can be null. `calls` counts calls that reached the missing function.
- `coverage` says which sections the runtime collected for this run. A list can be empty because nothing happened or because this build doesn't collect it. With a section's coverage off, don't read its empty list as "nothing missing".
- `truncated` counts the entries dropped from each capped list, so a capped list never looks complete.

Lists are sorted by count, highest first. They hold at most 200 entries (`missing.functions`, `shaders.unsupported`) or 100 (`imageFormats`, `descriptors`); strings are at most 300 characters. The whole file is at most 65,536 bytes of UTF-8. When it would be larger, the runtime drops the lowest-ranked list entries first, then optional descriptions, and never the build, game or stop details.

## Privacy

Only known, structured values are copied. Free-form text the runtime can't vouch for is left out (null). The file holds no usernames, home paths, hostnames, IP or MAC addresses, serial numbers, account IDs, save data or screenshots. Paths are relative to the AnyPS5 source tree.
