# Game testing

> **Draft.** The maintainers are asked what they need from this page in [Q&A #6622](https://github.com/boykopovar/AnyPS5/discussions/6622). Until they answer, treat every section as provisional.

Running a game is a big effort for the community: a legally dumped title, a suitable machine, hours of setup, often a PR build, and time spent watching it run. Many runs end early with a crash or a stall. Every run is still useful, so this page asks for as much information as possible from each one. A failed run with a complete log helps as much as a successful one.

For user options see [USAGE](../user/USAGE.md); for internals see [ARCHITECTURE](ARCHITECTURE.md).

## Before a run

- Build `main` or the PR head in a clean worktree, including the `libs` targets (they are not in the default build).
- Convert the title: `relinker --registry src/eboot.elf app.elf` on Linux, or `relinker --windows --registry src/eboot.elf app.exe` on Windows. Check the printed `Converted modules: N`.
- Convert every module the title loads from its own folders (`sce_module/`, `Media/Modules/`, `Media/Plugins/`). Plugins need `--module-dir`.
- Run `tools/import_audit.py` on the registries. Exit code 1 lists imports that will fail or throw.
- Run one title at a time. Titles share the GPU and the controller file.

## Launch

Linux:

```sh
APS5_PROFILE_DRAW=1 APS5_PROFILE_GPU=1 APS5_LOG_TIMESTAMPS=1 \
APS5_NO_MOUSE_CAPTURE=1 APS5_PAD_INPUT_FILE=/tmp/anyps5-pad \
setarch x86_64 -R timeout 600 ./app.elf > run.log 2>&1; echo "exit=$?"
```

Windows PowerShell:

```powershell
$env:APS5_PROFILE_DRAW = '1'
$env:APS5_NO_MOUSE_CAPTURE = '1'
$env:APS5_PAD_INPUT_FILE = "$PWD\pad.txt"
.\app.exe *> run.log
"exit=$LASTEXITCODE"
```

- `APS5_PROFILE_DRAW=1` prints `[present]` lines every 10 s. Without it a working title looks frozen.
- `APS5_NO_MOUSE_CAPTURE=1` stops the window taking the pointer; the middle button still toggles it.
- `setarch x86_64 -R` (Linux only) turns off address randomisation so addresses match between runs.
- On Windows, a `--windows-gui` build started without a console writes its output to `anyps5.log` beside the executable, replacing the previous run's file.

## What to read

| Check | Where | Meaning |
|---|---|---|
| Exit code | shell | Linux: 124 time cap, 134 uncaught exception, 139 segfault, 137 killed. Windows: the exception code (for example `0xc0000005`) |
| Last lines | `run.log` or `anyps5.log` | `terminate called`, `what()`, `presentation failed`, `ASSERT`, crash report |
| Frames | `[present] N presents over 10 s` | Steady N above 0 means frames are moving |
| Memory | `[texture] N storage images (X MiB)` | Growth before a `vkAllocateMemory ... -2` error |
| Queue waits | `[packets] ... WAIT_REG_MEM_64 ...` | A queue spending the window waiting on the GPU |
| Milestones | `Level has started: <name>` | How far the title got |
| Kernel (Linux) | `dmesg \| grep app.elf` | Fault address and instruction pointer |

## Stall, slowdown, or crash

- **Stall:** no `[present]` lines for a long time while other periodic lines keep coming. The process is alive, but no frames are presented.
- **Slowdown:** presents per 10 s far below earlier runs of the same scene. Record the scene (save file or level), since it changes the numbers.
- **Crash on Linux:** take the `ip` from `dmesg` and its offset in the image. Repeat the run under `gdb` (launched by gdb, not attached) for registers, the faulting instructions and all thread stacks.
- **Crash on Windows:** the crash report in the log lists the backtrace and registers. `APS5_CRASH_INDIRECT_MEMORY=1` adds up to 64 bytes from each pointer in the register dump. `APS5_HANG_DUMP_SECS=<n>` writes a dump after a hang of n seconds. `APS5_TRACE_EXIT=1` reports the process exit point.
- **Traces (heavy):** `APS5_TRACE_FRAMES=<n>:<m>` or `APS5_TRACE_SECONDS=<s>` writes a capture for `tools/trace_digest.py`. Traces slow the title and can move the crash; say so when reporting.

## Windows specifics

- **NVIDIA driver 610.47:** its shader compiler can crash while pipelines are created. The log ends in `FATAL: unhandled exception 0xc0000005` with `nvgpucomp64.dll` in `rip`. See [USAGE](../user/USAGE.md#gpu-selection).
- **Dozen (D3D12 through Vulkan):** some D3D12 drivers crash during Vulkan adapter enumeration. Set `$env:VK_LOADER_DRIVERS_DISABLE = '*dzn*'` only if a native Vulkan device exists.
- **Direct memory:** `sceKernelAllocateDirectMemory` commits memory when the title maps it, and the commit limit is installed memory plus page file. A title can fail here without any GPU error.
- **Trace imports:** an executable converted with `--trace-imports` records calls named in `APS5_TRACE_CALLS` (for example `libkernel,libSceSaveData` or `all`).
- **Debugger:** attach Visual Studio or WinDbg to `app.exe`. Find the process by its image name, not by the title's name.
- **Diagnostic writes:** `APS5_WATCH_ADDR` and `APS5_WATCH_WRITE` report writes to one guest address. They are for debugging only.

## Input

`tools/pad_input.py` writes presses to `APS5_PAD_INPUT_FILE`. Physical controllers are opened at start and when plugged in later; the log names them with `Pad: connected game controller`.

## Saves

Save data is kept under the runtime's `_sd_mem/` folder. Copy it into a new runtime to continue from the same point.

## Reporting a result

Include, in this order:

1. Commit (and each PR applied on top), and whether the tree was modified.
2. Title ID, region and version.
3. Host: CPU, OS (and kernel on Linux), the GPU as the log's `Selected GPU name=` line names it, and the driver version. Copy the GPU from that line, not from what the machine is expected to use: without presentation support (for example under Xvfb) AnyPS5 can select a software renderer such as llvmpipe, and on Windows a different adapter or Mesa Dozen.
4. The exact command and every `APS5_*` / `ANYPS5_*` variable.
5. Exit code and the last 200 log lines; the kernel line (Linux) or crash report (Windows) for a crash.
6. Presents per 10 s and storage-image lines if memory is involved.
7. How many runs and how long each lasted. One run is an observation, not a reproduction.

Keep AI-generated analysis separate from measured data, and say which is which.
