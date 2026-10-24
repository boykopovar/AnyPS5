# Game testing

Running a game is a big effort for the community: a legally dumped title, a suitable machine, hours of setup, often a PR build, and time spent watching it run. Many runs end early with a crash or a stall. Every run is still useful, so this page asks for as much information as possible from each one. A failed run with a complete log helps as much as a successful one.

For user options see [USAGE](../user/USAGE.md); for internals see [ARCHITECTURE](ARCHITECTURE.md).

## Before a run

- Build `main` or the PR head in a clean worktree, including the `libs` target (it is not in the default build). If a new test breaks the build, `cmake --build build --target libs relinker` is enough to convert and run a title.
- `tools/check_main_sync.py --fetch` in the worktree reports whether the branch is in step with upstream `main`, so a result isn't taken on an old base by mistake.
- Convert the title from plain ELF files (`tools/unwrap_self.py` unwraps a dump's `eboot.bin` and `.prx`):
  - Linux: `relinker --registry src/eboot.elf app.elf`
  - Windows: `relinker --windows --registry src/eboot.elf app.exe`. Without `--windows` the output is an ELF file whatever its name, and Windows refuses to start it (exit code 216, "not compatible with the version of Windows").
- A dump whose `app0` holds `ampr_assets.index` and `ampr_assets-*.pak` was repacked for [ampr_emu](https://github.com/drakmor/ampr_emu): the packed files were removed from `app0`, and only ampr_emu reads the packs. Restore them first with ampr_emu's `tools/ampr_pack.py unpack --index app0/ampr_assets.index --output <dir>` and copy the files missing from `app0`. Without them a Unity title stops at start-up on the missing `Media/Resources/unity default resources`.
- Convert every module the title loads from its own folders (`sce_module/`, `Media/Modules/`, `Media/Plugins/`). Unity titles need `--module-dir Media/Modules --module-dir Media/Plugins`. Put each printed `Guest module:` file next to its original `.prx` in `app0/`, and keep the original there.
- `libs/` holds only AnyPS5's own libraries, from `build/core/libs/libs` (not `unpatched/`). A game module placed there fails to load (error 193 on Windows). A self-built `libs/` on Windows also needs `libgcc_s_seh-1.dll`, `libstdc++-6.dll` and `libwinpthread-1.dll` from the toolchain's `mingw64/bin`, or every library fails with error 126 ([USAGE](../user/USAGE.md#input-and-conversion)).
- Run `tools/import_audit.py` on the registries. Exit code 1 lists imports that will fail or throw.
- Run one title at a time. Titles share the GPU and the controller file. Some titles start a second process (Peace Walker's `mgspw.mgspw.elf`) that can outlive a time cap on the first: check that every process of the previous run is gone.

## Launch

Linux:

```sh
APS5_PROFILE_DRAW=1 APS5_PROFILE_GPU=1 APS5_LOG_TIMESTAMPS=1 \
APS5_PAD_INPUT_FILE=/tmp/anyps5-pad \
setarch x86_64 -R timeout 600 ./app.elf > run.log 2>&1; echo "exit=$?"
```

Windows PowerShell:

```powershell
$env:APS5_PROFILE_DRAW = '1'
$env:APS5_PAD_INPUT_FILE = "$PWD\pad.txt"
cmd /c '.\app.exe > run.log 2>&1'
"exit=$LASTEXITCODE"
```

- On Windows PowerShell 5.1, `*>` writes the log as UTF-16 and wraps the first stderr line in an error record, so `grep` can't read it; let `cmd` do the redirection, as above. `$LASTEXITCODE` still carries the exit code.
- `APS5_PROFILE_DRAW=1` prints `[present]` lines every 10 s. Without it a working title looks frozen.
- `APS5_NO_PAD_INPUT=1` ignores keyboard and mouse, so a stray press doesn't move a measurement run.
- `setarch x86_64 -R` (Linux only) turns off address randomisation so addresses match between runs.
- On Windows, a `--windows-gui` build started without a console writes its output to `anyps5.log` beside the executable, replacing the previous run's file.
- On Windows the title needs a signed-in desktop. Started from an SSH session (session 0) it has no usable window. Start it inside the user's session instead, for example with a scheduled task that runs interactively. A newly created task does not start on battery power by default and fails silently; allow that in its settings.

## Check the GPU and the caches

- **GPU:** copy the GPU from the log's `Selected GPU name=` line. Without presentation support AnyPS5 can select a software renderer: under Xvfb (no DRI3) that is llvmpipe, without an error, and the title still runs. Set `ANYPS5_GPU` to part of the expected device name; the start then fails if that device can't be used. For headless Linux runs on a real GPU, Weston headless with the GL renderer plus Xwayland provides DRI3.
- **Caches:** first and later runs can differ a lot. On a slow GPU a first frame took minutes with empty caches and seconds on the next run. Two caches matter:
  - AnyPS5's `shader_cache/` beside the executable, or `ANYPS5_SHADER_CACHE_DIR`. `ANYPS5_NO_SHADER_CACHE=1` disables it.
  - The driver's own: Mesa (`MESA_SHADER_CACHE_DIR`, or `MESA_SHADER_CACHE_DISABLE=true`), AMD on Windows (`%LOCALAPPDATA%\AMD\VkCache`). Back a driver cache up instead of deleting it; it isn't AnyPS5's.

  Say in the report whether the caches were empty or warm. A first run that ends in device lost (`VK_ERROR_DEVICE_LOST`) or a GPU wait timeout while large pipelines are still being built, seen on AMD, often is the cache and not a regression: run again with the caches warm and report both runs.
- **Power and display:** on a laptop, run on AC power; a run on battery can make a neutral change look like a regression. Note the display's refresh rate: a title that asks for 120 Hz output can stop at start on a 60 Hz display. Keep the display awake for long or unattended runs: a title started while it sleeps stops with `SDL_GetWindowDisplayIndex failed: Couldn't find any displays`.
- **Memory budgets:** two budgets come from the machine's memory and can change the frame rate several times over, so check them before comparing results from different machines.
  - Host imports (`APS5_HOST_IMPORT_MIB`, see [ENVIRONMENT](ENVIRONMENT.md)): the default follows the physical memory and the host heap the driver reports, so two machines with the same RAM can get different budgets. Refused imports print `[gpu] host import budget: X MiB live of Y MiB (...); N imports (M MiB) refused so far`, at most every 10 s; without such a line nothing was refused. A refused range is copied on every later use instead, and those copies can take device memory the texture caches need.
  - Texture caches (`APS5_TEXTURE_CACHE_MIB`, see [USAGE](../user/USAGE.md#texture-cache-budget)): `[gpu] sampled texture cache budget N MiB` at start and when it changes, and with `APS5_PROFILE_DRAW=1` a `[texture-cache] (10 s)` line with the evictions and the budget. Thousands of evictions per 10 s with the budget at its 2048 MiB floor mean textures are rebuilt every frame.

  Report these lines. Set either variable only for a comparison, and say so.

## What to read

| Check | Where | Meaning |
|---|---|---|
| Exit code | shell | Linux: 124 time cap, 134 uncaught exception, 139 segfault, 137 killed. Windows: the exception code, `0xc0000005` access violation, `0xc0000409` uncaught exception or `abort()` (the log ends with `FATAL: abort() (SIGABRT) on thread N`) (PowerShell shows it signed, -1073740791) |
| Last lines | `run.log` or `anyps5.log` | `terminate called`, `what()`, `presentation failed`, `ASSERT`, `FATAL:` crash report |
| Frames | `[present] N presents over 10 s` | Steady N above 0 means frames are moving |
| Memory | `[texture] N storage images (X MiB)` | Growth before a `vkAllocateMemory ... -2` error |
| Queue waits | `[packets] ... WAIT_REG_MEM_64 ...` | A queue spending the window waiting on the GPU |
| Milestones | `Level has started: <name>` | How far the title got |
| Kernel (Linux) | `dmesg \| grep app.elf` | Fault address and instruction pointer |
| System log (Windows) | Event Viewer, System | A hard hang with nothing logged shows up afterwards as Kernel-Power 41 |

## Stall, slowdown, or crash

- **Stall:** no `[present]` lines for a long time while other periodic lines keep coming. Some titles present only when the view changes, so first send one input (`tools/pad_input.py`) and check whether presents resume.
- **Slowdown:** presents per 10 s far below earlier runs of the same scene. Record the scene (save file or level), since it changes the numbers, and check power and caches first.
- **Crash on Linux:** take the `ip` from `dmesg` and its offset in the image. Repeat the run under `gdb` (launched by gdb, not attached) for registers, the faulting instructions and all thread stacks.
- **Crash on Windows:** the `FATAL: unhandled exception ...` report in the log lists the faulting `rip` (for example `app.exe+0x...`), the registers and a stack scan. `APS5_CRASH_INDIRECT_MEMORY=1` adds up to 64 bytes from each pointer in the register dump. `APS5_HANG_DUMP_SECS=<n>` writes a dump after a hang of n seconds. `APS5_TRACE_EXIT=1` reports the process exit point.
- **Traces (heavy):** `APS5_TRACE_FRAMES=<n>:<m>` or `APS5_TRACE_SECONDS=<s>:<len>` writes a capture for `tools/trace_digest.py`. Traces slow the title and can move the crash; say so when reporting.

## Measuring performance

- Measure in gameplay, not in cinematics, menus or loading screens.
- Use the title's default settings, and say so. A changed graphics mode makes a number incomparable with everyone else's.
- Measure the same spot every time: load a save, wait for the scene to settle, then measure a fixed window with no input.
- Compare builds on the same machine, alternating them (A, B, A, B), at least two runs each. Throw away a run that was disturbed (another program, a sleeping display) and say so.
- `APS5_TRACE_SUMMARY_EVERY=10` writes one line per 10 s to `aps5-summary.jsonl` without slowing the title; `tools/trace_digest.py` turns it into a table, and `tools/performance_compare.py` compares two captures ([BUILD](BUILD.md#performance-trace)). Pull requests labelled `perf` need a capture of `main` and one with the change.
- `APS5_DUMP_FRAMES=<n>` with `APS5_DUMP_FRAMES_EVERY=<k>` saves frames as `frame_<index>.bmp` to check what was on screen. A second process of the same title writes to the same names and overwrites the first one's frames. While 64 MiB of frames wait to be written, the presenter waits for the
  writer, so a slow disk or `APS5_DUMP_SCALE=1` slows the title down instead of stopping it.

## Title notes

- **PlayGo:** dumps without PlayGo metadata stop at start unless `APS5_PLAYGO_UNLISTED_CHUNKS` is set; report the value used.
- **UE4 titles:** keep the startup movies in place. Renaming them to skip the intro has made a level render without its 3D view, which looked like an emulator bug and wasn't.

## Input

`tools/pad_input.py` writes presses to `APS5_PAD_INPUT_FILE`; they reach the title without focusing its window. Keys sent to the window instead (for example with `SendInput` on Windows) only arrive while it has focus. Physical controllers are opened at start and when plugged in later; the log names them with `Pad: connected game controller`.

## Saves

Save data is kept under the runtime's `_sd/` folder, and save-data memory under `_sd_mem/`. Copy them into a new runtime to continue from the same point.

## Reporting a result

Include, in this order:

1. The build: the `main` commit, each PR applied on top with its head, and any local change. A result from an integration branch should stay distinguishable after one of its PRs merges.
2. Title ID and content version, not only the game name.
3. Host: CPU, OS (and kernel on Linux), the GPU as the `Selected GPU name=` line names it, the driver version, AC or battery, and the display refresh rate.
4. The exact command and every `APS5_*` / `ANYPS5_*` variable, the in-game settings if they differ from the defaults, and whether the caches were empty or warm.
5. How far the title got (menu, a save, a scene) and what stopped it: the exit code and the last 200 log lines, plus the kernel line (Linux) or crash report (Windows) for a crash.
6. Presents per 10 s, and the storage-image and budget lines (`[gpu] host import budget`, `[gpu] sampled texture cache budget`, `[texture-cache]`) if memory is involved.
7. How many runs and how long each lasted. One run is an observation, not a reproduction.
8. The date of the test. Leave unknown fields empty rather than filling them from another report.

Keep AI-generated analysis separate from measured data, and say which is which.
