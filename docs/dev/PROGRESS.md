# Progress reporting

[`tools/progress.py`](../../tools/progress.py) generates `progress.json`, HTML, SVG maps and badges. The [Pages workflow](../../.github/workflows/pages.yml) publishes them with the web converter after pushes to `main` that change the converter, the web page or these tools. The [Pages progress workflow](../../.github/workflows/pages-progress.yml) runs after pushes that change `core/libs/prx` or the opcode list, compares the generated `progress.json` with the deployed one and dispatches the Pages workflow only when they differ, so the history chart gains new days only when the progress changes. The [Progress report workflow](../../.github/workflows/progress-report.yml) compares the base and head trees of pull requests.

## Library counts

The scanner reads `APS5_VABI` function definitions under `core/libs/prx/<library>/`, excluding `tests` subdirectories and their descendants. Test callbacks do not contribute to library counts. Definitions whose names end in `_nid_no_patch` or `_nid_no_patch_cut` are internal helpers and are not counted, unless an `APS5_EXPORT` in the library exports them under an explicit name. A definition is pending when its body calls `NotImplemented_nid_no_patch`, directly or through a recognized local stub wrapper. Call matching uses complete identifiers and accepts whitespace or comments before `(`. Other definitions are counted as implemented. Braces inside string literals, character literals and comments do not delimit bodies. These counts describe declared functions, not every firmware export, verified ABI compatibility or playable games. Silent behavior and unverified assumptions remain in [Technical debt](TechnicalDebt.md).

A library with no scanned definitions can include a sibling library's source builder. The scanner recognizes literal `include(${CMAKE_CURRENT_SOURCE_DIR}/../<library>/<file>.cmake)` references, including quoted paths. It does not evaluate arbitrary CMake expressions. It also recognizes a top-level `.cpp` that includes another library's source with `#include "prx/<library>/<file>.cpp"`.

For these wrappers, the JSON group adds `shared_sources` with the source library's directory name. Its own counts and name lists remain empty. The HTML table links to the source library instead of showing `0/0`; the source library's map tooltip lists its wrappers. Global totals count the definitions at the source library once.

A library with no scanned definitions and no shared sources, such as one whose `Export.cpp` only has `APS5_DUMMY_FUN` because its functions are implemented in another library, is listed as having no exports instead of `0/0`. It is left out of the map.

## History

[`tools/progress_history.py`](../../tools/progress_history.py) measures the first-parent history of `main` with the same rules as `progress.py` and writes `history.json`, `history.svg` and `history.html` (the chart with a tooltip per day plus a table, linked from the README and the end of the progress page). The chart plots counts, not percentages, so it shows how much is added over time: one panel for library functions and one for shader instructions, each on its own scale with the implemented count as a solid line and the declared total as a dashed one (for shaders the declared total is the instruction list in `tools/rdna_isa.txt`); the current percentage sits next to each panel's implemented label. It takes one point per UTC day, at the last commit of the day, and skips commits whose tree lacks the scanned paths. The chart compacts as days accumulate: up to 14 days it labels every day, up to 150 days one label per Monday, beyond that one per month; points are drawn up to 45 days, and every day keeps a hover tooltip, while the table and `history.json` keep every day. Old trees are measured with today's counting rules and instruction list, so the series is comparable but not what the page showed on that day. The workflow checks out the full history for it.

## Shader recompiler numbers

The shaders badge and the "Shader recompiler: counted from the code" section of the progress page come from [`tools/shader_scan.py`](../../tools/shader_scan.py). They are counted from the shader recompiler's code, not from playing games, so they say nothing about whether a game runs. The older "GPU shader instructions" figure counted instruction names that have an `RdnaOpcode` enumerator and reached 100%; the badge, the progress map panel ("Shader forms decoded") and the page now show the forms instead. That figure stays in `progress.json` under `shaders` for comparisons and history, and is shown only when the shader source scan cannot run (a local run without `--require-shader-source`).

The scan takes every encoded form of every instruction in AMD's RDNA 2 instruction set from [`tools/rdna2_isa_forms.txt`](../../tools/rdna2_isa_forms.txt). A form is the base encoding, the VOP3 encoding of a VOP1, VOP2 or VOPC instruction, or a DPP16 or DPP8 word on a VOP1 or VOP2 instruction. SDWA forms are not counted, because whether the decoder accepts one depends on selector bits that a source scan cannot tell apart.

- **Decoded:** the decoder's opcode tables under `core/shader/recompiler/RdnaDecoder/src` map the form to an `RdnaOpcode`, applying the same VOP3 opcode offsets and the VOP2 and DPP refusals as the decoder. The badge shows decoded forms out of all base and VOP3 forms.
- **Translated:** that `RdnaOpcode` has a `case` label in `TranslationContext::emitScalar`, `emitVector`, `emitMemory` or `emitInterpolation`. A case label shows the translator handles the opcode at all, not that the translation is correct.
- **DPP forms** are reported separately.
- **Unsupported throws:** `throw` statements under `core/shader/recompiler` (tests excluded) whose message or exception type says unsupported or not implemented.

The "Still needed" panel under the progress map, and the matching section of the progress page, list what is left, all read from the same scan:

- the forms the decoder does not decode (base and VOP3 forms, then DPP forms separately) and the decoded opcodes the translator has no `case` for, by name, grouped by encoding and form;
- the unsupported-throw sites as bars per recompiler area (the first directory under `core/shader/recompiler`; top-level files are "Recompiler"). `progress.json` carries the per-site file, line and message under `shader_source.throw_sites`;
- image formats as bars: sampled and typed-buffer formats from the recompiler's format tables (`IrBufferFormat`, `DefinedImageFormats`, `kFormatInfoTable`, `GetFormatInfo`), and formats the driver maps to a Vulkan format in `libSceAgcDriver/Graphics/src/TextureFormat.cpp` (`kFormatLookup` and `remapGuestFormat`).

When the recompiler is restructured so a table or function cannot be found, the scan stops with an error naming it. The Pages workflows pass `--require-shader-source`, so nothing is published from a failed scan; without the flag `progress.py` prints a warning and leaves the shader source numbers out.

`tools/rdna2_isa_forms.txt` is derived from AMD's machine-readable ISA XML (`amdgpu_isa_rdna2.xml`, published under the MIT licence; the file keeps AMD's copyright and permission notice). To refresh it for a newer release, run `python3 tools/progress.py --regenerate-isa-forms amdgpu_isa_rdna2.xml`.

## Comparisons

Comparisons identify functions by library and name. When a head group becomes a shared-source wrapper, its previous definitions are compared under the source library's identity. Removing a duplicate implementation is reported as sharing sources, without reporting the retained functions as removed.

Real removals and transitions back to stubs remain visible. Percentages retain the original snapshot totals, so eliminating duplicate accounting can change a percentage without changing available functionality. Older JSON snapshots without `shared_sources` are accepted.

PR reports include a table of changed libraries with implemented export counts, declared totals, percentages before and after, and the change in percent (`%`), the difference between the two percentages, written with the same unit as the headline of the report. The table uses the same two JSON snapshots as the global report. A library appears when its counts, export names or shared-source owner change, including when its counts stay equal. Unchanged libraries are omitted, and the table lists at most 100 libraries in name order with a count of omitted rows.

New or removed libraries show `Not present` on the missing side. Libraries whose snapshots contain both counts and report zero exports show `No exports`; if either count is absent, the table shows `Counts unavailable`. Wrappers show `Shared sources` with the source library's name. These rows show `n/a` for the percentage change. The table retains the scanner's counting rules; it does not measure compatibility.

Implemented definitions are also classified as behaviourally empty when their body holds only `(void)name;` discards and at most one constant return, bare `return;` or `throw`, including a return of a local wrapper that is itself behaviourally empty; wrapper calls whose arguments contain calls do not qualify. Comparisons list newly implemented behaviourally empty functions without changing any count. Older JSON snapshots without `inert_names` are accepted.

## One library

`--module NAME` limits the library totals to those directories under `core/libs/prx`. Repeat the flag to include several. A name that is not a directory is an error. Shader counts stay complete. A shared-source wrapper is included only when its own directory is named. `--json` prints the object written to `progress.json` on stdout, instead of the two summary lines. Either flag makes the output directory optional. Without them the directory is still required, and a run that names only the directory still writes the whole tree.

## Verification

```sh
python3 tools/tests/test_progress.py
python3 tools/progress.py /tmp/anyps5-progress
python3 tools/progress.py --module libScePad
python3 tools/progress.py --json
python3 tools/tests/test_progress_history.py
```

The regressions check that adding test callbacks leaves counts and comparisons unchanged, including when a callback shares a pending function's name or a wrapper uses shared sources. They also create a source library and a copied wrapper, move the wrapper to a shared builder, check output labels and totals, then verify real regressions and removals are still reported. With `BUILD_TESTING=ON` and Python available, CTest registers them as `progress_report`.
