# Run report

`anyps5-report.json` is a structured summary of one run. [`run-report.schema.json`](../run-report.schema.json) is the single source of truth for its shape; the sample reports in [`run-report-fixtures/`](../run-report-fixtures/) all validate against it. This is version 1. Adding a field keeps version 1; renaming or removing a field, or changing what one means, is version 2.

Print a readable summary of a report with:

```sh
python3 tools/run_report_summary.py path/to/anyps5-report.json
```

The summary includes the run result, the furthest observed stage, rendering counts, system details, and available missing-function and shader diagnostics. It says when a section was not collected or a list was truncated. It reads the local report and does not upload it.

The file stays on your machine. Nothing is uploaded unless you share it.

## Switching it off

The report is written by default. Turn it off with the launch flag `--aps5-report=off` or the environment variable `APS5_REPORT=0`. The flag wins over the environment variable, and the environment variable wins over the default (on). There is no ini setting. The flag needs a game converted with a new enough relinker; the environment variable also works with older conversions. The runtime reads the flag from the command line and does not remove it, so the game can see `--aps5-report=...` in its arguments. When the game relaunches itself, the choice is passed on through `APS5_REPORT`.

## Where it goes

`anyps5-report.json` is written next to the game's executable. If that folder can't be written, it goes to the AnyPS5 data folder under `reports/<run id>/` and the runtime prints where. `run.id` is that same id: 32 lowercase hex characters, random per run.

The report is written once, when the run ends through the guest's `exit` or `_exit`, and on Linux and macOS also when the entry returns or the host calls `std::exit`. On Windows only `exit` and `_exit` write a report: a return from the entry writes none, because exit handlers of a DLL run inside the loader lock. The Windows and macOS code and the junction test have not been run. A run that stops on an exception nothing catches (`std::terminate`), such as a call to a function that is not implemented, writes the report with exit `abort` before the terminate handler runs, or after it when the game installed its own handler. A run that is killed or crashes writes no report.

## What `anyps5.modified` means

`false` means the build's git tree had no change to tracked files, no untracked files that git does not ignore, and no change inside a submodule. `true` means any of those. `null` means the build did not come from a git checkout. A mixed set of libraries from different builds is not detected.

## Unknown is null

A value the runtime cannot know is `null`, never a guess. This applies to `anyps5.*`, every `game.*` and `system.*` field, the whole `system.gpu` object `run.exitCode`, `stop.source`, and instruction details. Read null as "unknown", not as false or zero.

Every text value in the report goes through the same filter for user names, host names, addresses and account numbers. A version made of four numbers separated by dots (`1.2.3.4`) looks like an IPv4 address, so it is left out (null) rather than risk writing an address; versions of three numbers (`01.002.000`) and release tags (`v0.1.3`) are kept. The commit and the run id are fixed-length hex and are checked by their pattern only, so the build identity is always there.

`game.titleId` is copied only when it matches `PPSA` or `CUSA` style (four capital letters and five digits); otherwise it is null.

## Run

| Field | Meaning |
|---|---|
| `stage` | The furthest milestone the runtime observed. `unknown` is the starting value. The runtime never claims `converts`: conversion happens in the converter. |
| `loads` | The runtime reached the game's entry point. This does not prove every import resolved, because imports can load later. |
| `starts` | Game code was entered on its thread. |
| `first-draw` | A real game draw or dispatch was submitted to the GPU. |
| `presents` | The GPU accepted a game frame for display. This does not prove the frame was visible or not black. |
| `milestones` | Seconds since start. Milestones not reached are left out. |
| `exit` | `clean`, `crash`, `abort` or `killed`. A non-zero exit code from the game is not automatically a crash. |
| `exitCode` | The process exit code when known, otherwise null. |
| `averageFps`, `fpsWindowSeconds` | Frames per second measured from the first presented frame to the last, and the length of that window. Both are null with fewer than two presented frames. |
| `frameMs` | Frame-time percentiles in milliseconds: `p50`, `p90` and `p99` of the intervals between presented frames, over the most recent 2,048 intervals. Null with fewer than two presented frames, or when milestones are not covered. The percentile is nearest-rank, the same math the optional performance trace uses. |
| `recovered`, `incomplete` | `incomplete` is true when an observation had to be dropped. |

## Run recipe

Next to `anyps5-report.json` the runtime writes `anyps5-recipe.json`, a small JSON file that records how the run was started, so a report can be reproduced. It holds `run.id`, `anyps5.commit` (the build commit, or null), `argv` (the process arguments), `environment` (only the `ANYPS5_*` variables that are set), and `configFiles` (the file names of configuration files the run was given, for example the input mapping, without their folders).

Values pass through the same text filter as the report, so a path or a user name shows as `[redacted]`. A variable whose name contains `TOKEN`, `KEY`, `PASSWORD` or `SECRET` is never written, whatever its value. The recipe is written only when the report is written; the same rules about killed and crashed runs apply.

## Stop

`stop.kind` is `none` and `stop.reason` is `guest-exit` for a run that ended through the exit path. A run that ended in `std::terminate` has `stop.reason` `runtime-stop`: `stop.kind` is `missing-function` with the function and its `source` when a function that is not implemented threw, otherwise `other` with the exception's text as the message. The schema lists the other kinds and reasons; this build writes only these.

## What `system` holds

`system.osVersion` is the operating system name and version (on Linux the distribution's `NAME` and `VERSION_ID` from `os-release` and the first three numbers of the kernel release, for example `Ubuntu 24.04 (kernel 5.15.153)`; whatever the vendor or the user appended to the kernel release is left out), `system.cpu` is the CPU model name (without a trailing clock speed such as ` @ 3.50GHz`), `system.ramGiB` is the installed memory in whole GiB, and `system.gpu` is the graphics device the runtime selected: `vendor` (from the PCI vendor id: `AMD`, `NVIDIA`, `Intel`, `Apple` or `Other`), `name`, `driver` (the driver's own name and version text) and `vulkan` (the API version of the device). `gpu` is set when the device is selected, so it is null in a report of a run that stopped before that. These are meant to be model and version names, and they are filtered like every other text: a string that contains your login, your home folder's name, an address, an account number, a UUID, a path, an e-mail address or a serial-number-like token is left out of the report (that field becomes null). Your host name is compared too, with one exception: in `system.osVersion` only, a host name is allowed when it equals the identifier of a public distribution from a fixed list in the source (`ubuntu`, `debian`, `fedora`, `arch`, `cachyos`, `manjaro`, `nixos` and so on) and the `ID` of the machine's `os-release` is that same entry (a machine called `cachyos` running CachyOS keeps `CachyOS`). The `ID` is text its owner can edit, so it counts only when it parses (matching quotes or none) and is on the list; `NAME` and `ID_LIKE` never count. A host named after a distribution that is not on the list, or whose `os-release` says something else, loses the OS name from `system.osVersion`. An `os-release` value with an unmatched quote is not used, and the OS version then falls back to what is left (for example `Linux (kernel 6.8.0)`). In `cpu`, `gpu.name` and `gpu.driver` every name is compared. A login is never given that allowance, so a login that equals a word of the distribution's name (`arch` on Arch Linux) makes `system.osVersion` null. Any machine string that contains your login is cleared as a whole, so a login `arc` also clears `Intel Arc`. The serial-like rule (twelve or more letters and digits with both) also drops a driver string that carries a long git hash, as development builds of Mesa do.

## Which milestones a system records

| System | `loaded`, `started` | `firstDraw`, `firstPresent` | `stage`, `framesPresented`, frame rate |
|---|---|---|---|
| Linux | never recorded | recorded | recorded |
| Windows | never recorded | recorded | recorded |
| macOS | recorded | recorded | recorded |

`coverage.milestones` is true on every system; it means the stage, the milestones the system records and the frame counts were collected. Read a missing `loaded` or `started` on Linux or Windows as "this system does not record it", never as "not reached": a report with `stage: first-draw` and no `loaded` is a run that drew. The report says only what was observed: any other milestone that is missing may mean the run did not reach it, and it may mean the system does not record it, so read the table before you read a gap. `firstDraw` in particular is not set by compute work that is submitted and waited for on its own.

## What the runtime counts

- `milestones.firstDraw` is set when a batch that carries a recorded guest draw or dispatch has been accepted by the GPU queue. A guest draw counts in either kind of batch the runtime submits; a guest compute dispatch counts only when it is recorded into the runtime's main batch (a dispatch that is submitted and waited for on its own does not set the milestone). `firstPresent` and `framesPresented` count guest display buffers that the swapchain queue accepted (a suboptimal one too); internal clears, test pixels and rejected presents are not counted. `loaded` and `started` come from the entry bridge of the converted executable; only the macOS bridge reports them so far, so on other systems those two are left out.
- `missing.functions` counts calls to a function the runtime does not implement. The library is the folder the stub is compiled in; a call from older code that does not say where it is made has a null library. The name is the exported name without the runtime's suffix; a name that does not fit the schema is not listed and is counted in `truncated.functions`.
- `shaders.compiled` and `failed` count compilations where they happen (once per variant); reuse of a variant, in memory or from the disk cache, counts as a cache hit.
- `shaders.compileMs` is the total wall time spent in those compilations, failed ones included, in milliseconds. `shaders.slowCompiles` counts compilations that took more than 100 ms. Both are left out when shader coverage is off. These are measured on the machine that ran the game, so compare them only on the same machine.
- `shaders.unsupported` lists instructions the recompiler could not translate, by encoding family and numeric opcode (the name is null). It also lists instructions met in shaders that were prepared and never used, so an entry does not by itself mean the run stopped on it; the stop says that.
- `shaders.imageFormats` counts guest format lookups, internal ones included, with the use shown as `other`, plus the formats the runtime rejected. It measures how often a format was looked up, not how many images used it. `shaders.descriptors` counts the bindings of shaders as they were prepared for a draw; the count measures use, not distinct shaders.
- `truncated` counts entries (a function, an instruction, a format, a binding kind) that were dropped, once each however many calls they made. When a hook had to give up an observation because the collector was busy, the matching count is at least 1 and the report is marked `incomplete`.
- Nothing in this list is collected, counted or scanned when the report is off.

## Lists, coverage and truncation

- `missing.functions[]`: `library` can be null. `calls` counts calls that reached the missing function. The first one usually ends the run, so 1 is common; this is not a list of everything unimplemented.
- `shaders.compiled` and `failed` count real compilations; cache hits are not compilations (`cacheHits` is optional). `unsupported[]` has `encoding`, `opcode` (name) and `modifiers`, each null when the failure can't be pinned to one guest instruction. An unknown numeric opcode keeps its number in `opcodeValue` with a null `opcode`.
- `shaders.imageFormats[]` and `descriptors[]` have `status` (`accepted`, `rejected` or `unknown`), `usage` and `count`. `count` counts checks, not pixels. Accepted does not prove every use works.
  - Image format `usage`: `sampled`, `storage`, `color-target`, `depth-target`, `transfer`, `other`.
  - Descriptor `usage` is the access: `read`, `write`, `atomic`, `other`.
- `coverage` says which sections the runtime collected for this run. A list can be empty because nothing happened or because this build doesn't collect it. With a section's coverage off, don't read its empty list as "nothing missing".
- `truncated` counts the entries dropped from each capped list, so a capped list never looks complete.

Lists are sorted by count, highest first. They hold at most 200 entries (`missing.functions`, `shaders.unsupported`) or 100 (`imageFormats`, `descriptors`); strings are at most 300 characters. The whole file is at most 65,536 bytes of UTF-8. When it would be larger, the runtime drops the lowest-ranked list entries first, then optional descriptions, and never the build, game or stop details.

## Privacy

Only known, structured values are copied. Free-form text the runtime can't vouch for is left out (null). The file holds no usernames, home paths, hostnames, IP or MAC addresses, serial numbers, account IDs, save data or screenshots. Paths are relative to the AnyPS5 source tree.
