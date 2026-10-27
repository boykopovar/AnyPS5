# Tracing

Set a switch before starting the emulator. Unless noted, any value enables it. Most output goes to the emulator log or standard error. Performance captures are written to files as described in [Build](BUILD.md#performance-trace). Traces can be large and can affect timing.

## Kernel and libraries

| Switch | Output |
| --- | --- |
| `APS5_TRACE_AFFINITY` | Thread role, host thread ID, requested CPU mask and whether pinning succeeded. |
| `APS5_TRACE_AJM` | Submitted audio jobs, decoded output and unimplemented AJM calls. |
| `APS5_TRACE_APR` | APR file reads, including path, offset, size and destination. |
| `APS5_TRACE_AUDIOOUT` | Audio output port parameters when a port is opened. |
| `APS5_TRACE_AUDIOOUT2` | AudioOut2 context and port parameters, attributes, pushes, queue polls and periodic queue summaries. |
| `APS5_TRACE_CALLS` | On Windows, Linux and macOS, records calls to the imports selected by this comma-separated library list (or `all`); crash and abort reports (abort only on Linux and macOS) include the last 64 calls and most-called imports. The executable must be converted with `--trace-imports`; see [Usage](../user/USAGE.md#tracing-system-library-calls). |
| `APS5_TRACE_CALLS_FILE` | With `APS5_TRACE_CALLS`, a file rewritten every 10 seconds with every traced import called so far, most called first: number of calls, library and import name. |
| `APS5_TRACE_EXIT` | Guest `exit` and `_Exit` calls and process-exit backtraces. |
| `APS5_TRACE_FIBER` | Fiber initialization, switches and returns. |
| `APS5_TRACE_MEMORY` | Direct-memory allocation, mapping and protection operations. |
| `APS5_TRACE_PREPARATION_WAIT` | Shader preparation tasks moved to the front of the queue to satisfy a waiting caller. |
| `APS5_TRACE_SSE4A` | Each emulated SSE4A instruction once per guest instruction address (Windows). |
| `APS5_TRACE_SYNC` | Event-flag operations and their wait and wake results. |
| `APS5_TRACE_TIMERS` | Timing measurements for the host wait primitives used by the emulator. |
| `APS5_TRACE_USLEEP` | Every 2,000 guest `usleep` calls, with caller addresses and requested delays. |
| `APS5_TRACE_VIDEODEC` | Video decoder API calls and decode results. |
| `APS5_TRACE_WAITS` | Blocking waits aggregated by guest call site, including timeout counts and longest waits, every three seconds. |

## GPU and memory

| Switch | Output |
| --- | --- |
| `APS5_TRACE_BIGBUF` | Each copied or written-back guest-memory range of at least 1 MiB. |
| `APS5_TRACE_BDA` | Shader memory accesses that make a program use the buffer-device-address path. |
| `APS5_TRACE_BUFFER_WRITTEN` | Guest buffer elements classified as writable or read-only after descriptor population. |
| `APS5_TRACE_CAPSYNC` | Waits for GPU writes needed by CPU accesses, including submission and range details. |
| `APS5_TRACE_CONTEXT_STATE` | PM4 context-state operations and registers written while context state is pushed. |
| `APS5_TRACE_COPY_HLE` | Pending host-side copies, their ranges, GPU batches and known writers. |
| `APS5_TRACE_DCC_KEYS` | Display and storage-image DCC key reads, writes, fills and clears. |
| `APS5_TRACE_DISPATCH_CACHE` | Compute dispatch cache hits, misses and changed user-data words. |
| `APS5_TRACE_DISPATCH_IO` | Compute dispatch queue, submission serial, shader and resources; value `2` also prints small user-data buffers. |
| `APS5_TRACE_DRAW_CACHE` | Draw-cache lookups, hits, misses, and key differences, reported every ten seconds. |
| `APS5_TRACE_DRAWS` | Draw inputs and per-draw timing phases. With `APS5_PROFILE_DRAW`, also prints the detailed draw lines. |
| `APS5_TRACE_DRAW_PROGRAMS` | One line per draw listing selected shader stages, roles, code addresses and entry addresses. |
| `APS5_TRACE_FILL_COVER` | Initial fill/image coverage mismatches and refused image-layer clears. |
| `APS5_TRACE_FLUSH` | Storage images flushed to guest memory and ranges left for the CPU. |
| `APS5_TRACE_GPU` | Queue submissions, packet execution, waits and display-buffer scanouts. |
| `APS5_TRACE_HOST_IMPORT` | Host-import mapping attempts, memory-map details, failures and retirement. |
| `APS5_TRACE_IMAGE_TABLE` | Image-table resource materialization and descriptor selection. |
| `APS5_TRACE_INDIRECT_DRAWS` | Indirect draw packets that pass the draw precheck. |
| `APS5_TRACE_LABEL` | A matching `RELEASE_MEM` when recorded, and nearby `RELEASE_MEM`, `WRITE_DATA`, `COPY_DATA`, `DMA_DATA` and `WAIT_REG_MEM` packets when executed. Set the value to the guest address in hexadecimal. |
| `APS5_TRACE_LATE_LABELS` | GPU label writes observed after their waiting submission was handled. |
| `APS5_TRACE_QUERY` | Every sixteenth guest-memory page query, including cost and caller address. |
| `APS5_TRACE_RESOURCE_KEY_MISSES` | Resource-content key changes that cause draw or dispatch resource-cache misses. |
| `APS5_TRACE_SHADOW` | Unit-shadow publish, seed, eviction and retirement events; a number limits publish output. |
| `APS5_TRACE_SKIPS` | GPU operations skipped by diagnostics, with a bounded number of recompile-request dumps per reason. |
| `APS5_TRACE_STAGING` | Distinct guest-buffer ranges admitted to the write-staging window. |
| `APS5_TRACE_TEXTURES` | Sampled texture addresses, dimensions, format and nonzero-byte counts. |
| `APS5_TRACE_UNREADABLE` | Call sites that check inaccessible guest-memory ranges or fail to read them. |
| `APS5_TRACE_UPLOAD` | Reasons storage-image data is uploaded again. |
| `APS5_TRACE_BARRIERS` | Set to a positive integer to print that many skipped barrier decisions and their ranges. |

## Performance captures

See [Build](BUILD.md#performance-trace) for the trace format, summary fields and capture limits.

| Switch | Output |
| --- | --- |
| `APS5_TRACE_FRAMES=<start>:<count>` | Records a window of presents by zero-based frame number. |
| `APS5_TRACE_SECONDS=<start>:<length>` | Records a window of presents by elapsed seconds after the first present. Cannot be set with `APS5_TRACE_FRAMES`. |
| `APS5_TRACE_SUMMARY_EVERY=<seconds>` | Writes periodic summary lines without recording trace events. Can be combined with either window switch. |
| `APS5_TRACE_EVENTS=<count>` | Sets the per-thread event-buffer capacity for a frame or seconds window (1–4,194,304; default 262,144). |
| `APS5_TRACE_DIR=<path>` | Writes performance capture and summary files to this directory instead of the working directory. |
| `APS5_AB=<seconds>:<switch>[+<switch>...]` | Alternates phase A (the environment's settings) and phase B (the listed switches toggled) at flip packets within one run, each phase at least `<seconds>` long. Every phase change prints `[ab] phase A\|B <k> begins at present <n> (t <s> s)` and, `APS5_AB_SETTLE` seconds (default 2) later, `[ab] phase A\|B <k> settled at present <n> (t <s> s)`; `t` counts from the first flip. `APS5_AB_START=<n>` starts at flip `<n>`, `APS5_AB_START=<n>s` `<n>` seconds after the first flip. Only `APS5_NO_BARRIER_BATCH` can be listed; any other name fails the first flip. `python tools/ab_split.py <log>` prints the mean of each periodic report (`[present]`, `[gputime]`, `[draw] pipelined`, `[syncwait]`) per phase, counting only reports that lie wholly inside a settled phase, and each B phase against its neighbouring A phases. |
| `APS5_VIDEOOUT_TELEMETRY_DIR=<path>` | Writes the presents and the pad changes of a run to `<path>/videoout.jsonl`; see [Usage](../user/USAGE.md#video-out-and-pad-telemetry). |

## Shader registers

`APS5_TRACE_SHADER_REGS=all` prints register snapshots for every shader. Set it to a shader code address in hexadecimal to print one shader's registers.
