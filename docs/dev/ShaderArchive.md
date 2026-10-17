# Native shader archive tool

`agc_shader_archive` extracts Unreal IoStore shader libraries, constructs static
AnyPS5 requests, emits and validates SPIR-V, and compiles it with the selected
Vulkan device. Extraction and compilation run in C++ with bounded worker counts.
The existing production recompiler is reused. Oodle Mermaid streams are decoded
by the decoder in `Oodle.cpp`; see technical debt for the stream forms that are
not implemented.

## Build

From the AnyPS5 repository, using the existing configured Release build:

```powershell
ninja -C build agc_shader_archive shader_archive_tests -j 16
ctest --test-dir build -R '^shader_archive$' --output-on-failure
```

On Windows, put the compiler runtime directory and
`build/core/libs/libs/unpatched` on this shell's `PATH`. The executable is
`build/tools/shader-archive/agc_shader_archive.exe`. This target matches the
project's MinGW exception runtime and does not enable the global SPIR-V optimizer.
SPIRV-Tools validation runs in process. The Unix implementation uses mmap and
the Vulkan loader; Windows is the verified platform.

## Extract and compile

Paths below are examples. Each output directory must be new.

```powershell
$tool = '.\build\tools\shader-archive\agc_shader_archive.exe'
& $tool device --validation
& $tool extract --paks PATH_TO_PAKS --out extracted --jobs 16
& $tool build --extracted extracted --sample 16 --out first --jobs 16 --validation
```

`build --paks PATH_TO_PAKS` fuses extraction and compilation in memory.
`build --extracted` accepts this tool's own `extract` output. Both routes
create `programs.jsonl` and `programs.idx`. The JSON preserves all library
aliases; the compact index stores one verified byte location per unique
code/header identity.

For iteration, put full program IDs or code SHA256s in `selection.txt`, one
per line. Program IDs are SHA256 of the concatenated hexadecimal code and header
SHA256 strings.

```powershell
& $tool build --extracted extracted --catalog first\programs.idx `
  --select selection.txt --cache shader-cache --out iteration --jobs 16
```

Program-ID lookup uses binary search over mapped fixed-size records. Only the
selected groups are read, and selected code/header bytes are checked against the
index before use. Code-hash selection scans the small index to retain every
matching header variant. `--sample N` samples deterministically;
`--limit N` takes the first N eligible jobs. `--native-target 14` selects primary
variants, `15` secondary variants, and `0` both (the default).

Use `--offline --profile profile.json` for CPU translation and validation
against an explicit static-target profile. `replay --requests paths.txt`
accepts existing serialized requests; in offline mode their recorded targets
are preserved. Supply a matching profile for the validation environment.

The optional cache stores **validated** compiled artifacts. Its key includes
the compiler source version, request/code/ABI/target, compiler switches, and
validator version. Corrupt or incompatible entries are recompiled. Entries are
published atomically. Rebuilding the executable is expected and does not require
an executable SHA256 pin. Omit `--cache` to force translation and validation.

## GPU artifacts and results

Compute shaders go through `vkCreateComputePipelines`. Vertex and fragment stages
go through graphics pipeline library compilation. Mesh stages use
`vkCreateShadersEXT` when supported: the RX 9070 XT driver faulted when compiling
these standalone mesh library parts, while shader objects compiled all four
sampled mesh programs with zero validation errors. Other devices fall back to
mesh pipeline libraries. Rasterization and shader outputs are retained.

* `results.jsonl`: one result per attempted shader, hashes, phase times and status.
* `failures.jsonl`, `failures/*.req`, `failures/*.log`: classified failures and reproducers.
* `report.json`: planned/selected/attempted counts, cache hits, process CPU time,
  peak resident memory, aggregate worker timings and elapsed wall time.
* `profile.json`, `device.json`: target/device/driver and binary compatibility information.
* `pipeline-cache.bin`: Vulkan pipeline cache data for compute and graphics stages.
* `gpu/*.bin`: opaque native mesh shader binaries; use the recorded shader binary
  UUID/version and device configuration when loading them.
* `--retain`: also saves passing `.req`, `.spv` and native `.artifact` files.

Worker phase times overlap when parallel; they must not be added to elapsed wall
time. Cache hits reuse recorded validation rather than running the validator again.
`--validation` enables Vulkan validation layers and makes any validation error
fail the command. Shader compilation failures are indexed; exit 0 means the batch
finished, not that every shader passed. Fatal setup errors return 2. A driver crash
leaves `results.jsonl.partial` and the last per-job `GPU ...` log lines for triage.

These are static stage artifacts for the recorded interface assumptions, not a
test of rendered pixels or every linked draw variant. Unpaired geometry and
tessellation programs remain `needs_configuration`. Supported containers are
unencrypted, single-partition IoStore v8 with shader library v1 and AGC v24 data.
The decoder handles the observed Mermaid/Selkie stream forms and explicitly
rejects other Oodle codecs, entropy array types and checksum modes.

`--friendly-names` restores the slower validator name-generation path for
comparison or diagnostics; all validation rules remain enabled in both modes.
GPU cache state is reported separately from artifact cache state.

For first-time driver compilation measurements, set a new `APS5_PIPELINE_SALT`
value in the invoking process. This adds an unused specialization constant to
give the driver a new pipeline cache key without changing shader instructions
or deleting caches. The salt is recorded in the device report. Use a new
`--cache` directory as well to measure a complete first build; reuse both for
a repeat-build measurement.

## Integration

`Archive.hpp` owns extraction, `Native.hpp` owns verified index lookup and static
request construction, and `Gpu.hpp` owns device compilation. The core addition is
`CompileShaderUncached` in `Optimization/ResourceProgram.hpp`: it returns the same
`CompiledVariant` used by the renderer without retaining each source in a global
cache or materializing resource bindings. Optional frontend/backend timings apply
only to this entry point.

## Automatic runtime loading

Build with `--publish <emulator>/shader_cache/prebuilt/game.pack` to publish a
read-only pack directly, without writing individual retained shader files. This
works with `--paks` (direct game containers), `--extracted` and `replay`. The game
finds packs in `shader_cache/prebuilt` automatically on its next launch. The
emulator itself reads the compiled pack; IoStore extraction remains in this tool.

Existing `.artifact` outputs, normal cache folders and this tool's validated
cache folders can be converted without recompilation:

```powershell
& $tool pack --cache compiled-folder --out pkgout\shader_cache\prebuilt\game.pack
```

Publication validates every entry. Stale source versions, damaged files and
incompatible formats are counted as rejected; an entirely incompatible folder
fails without replacing an existing pack. The validator receipt is removed from
tool cache keys during conversion, preserving the complete production key.
There is no hash of the executable. Compiler source version and runtime ABI,
code, target capabilities, binding layout and static stage configuration must
still match. A standalone archive variant may therefore miss a live request;
replaying captured requests with `--offline` preserves their actual targets.

The renderer maps each pack once, checks its small sorted index, and reads only
requested entries from the mapped pack. Full keys and payload checksums are
checked on a hit. Misses or rejected entries fall through to the existing
writable cache and compiler. No all-shader decompression, directory scan or
cache copying occurs per draw. The emulator reads packs and loose entry files
only from `shader_cache/prebuilt` next to the writable cache; there is no
environment-variable override. Publish before launching; running processes
keep their current mapped snapshot.

`[shader-prebuilt]` logs startup mapping time, the first exact hit, and interval
hits/misses/rejections with lookup, file-read, decode, total and maximum load
times. These are CPU costs; mapped payload page faults are included in decoding.
Normal resource-plan preparation and per-draw specialization still run as needed.
The existing `[shader-disk-cache]` aggregate remains available. For a bounded
read-only coverage/timing check, use the same environment as the game:

```powershell
& $tool probe --requests captured-requests.txt --limit 16 --out load-times.json
```

Mesh shader-object binaries remain standalone tool outputs; the renderer uses
its normal mesh pipeline path and matching SPIR-V artifacts. A GPU build also
writes `pipeline-cache.bin` for inspection; importing it into the renderer's
pipeline cache is not implemented.

`Oodle.cpp` is an original implementation of the Mermaid/Selkie bitstream
layout published by the MIT-licensed oozextract 0.5.5 decoder. No vendored
decoder code is built by this tool.
