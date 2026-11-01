# Environment variables

The code reads environment names through `getenv`, `IsEnvironmentSet`, `kibSetting`, `MaskFromEnvironment`, `EnvInt`, `GetEnvironmentVariableA`, `GetEnvironmentVariableW` and `_wgetenv` with a wide literal, `configured`, `PipelineSwitch`, `environmentCount`, `EnvironmentNumber` and `addWideString` (the wide names the Windows entry stub reads). This table lists every `APS5_*` and `ANYPS5_*` name it reads, its kind, the value used when it is unset and what it does, one table per library. Regenerate the names with `python3 tools/env_reference.py --table`; the Conventions check fails when a name the code reads has no row here, or a row's name is no longer read.

### core/libs

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_GUEST_ENV_TEST_4C27` | string | `core/libs/tests/GuestEnvironment.cpp` |  | test variable that the guest environment test sets, copies and removes; not a product setting |
| `ANYPS5_JOB_MANAGER_CASE` | string | `core/libs/tests/JobManagerReference.cpp` |  | selects the CPU fixture run by `tools/run_job_manager_reference.py` |
| `ANYPS5_JOB_MANAGER_HOST` | presence | `core/libs/tests/JobManagerReference.cpp` |  | runs the reference adapter against the host fallback instead of the loaded guest module |
| `ANYPS5_LOADEXEC_PARENT` | string | `core/libs/tests/GuestLoadExecHost.cpp` |  | variable the Windows entry stub reads; its wide name is built in `core/relinker/elfpatcher/src/windows/WindowsEntryStubBuilder.cpp` |
| `ANYPS5_LOADEXEC_TEST_ROOT` | string | `core/libs/tests/GuestLoadExecHost.cpp` |  | directory the load-exec host test reads its guest executable from |
| `ANYPS5_POSIX_SPAWN_ROLE` | string | `core/libs/tests/GuestPosixSpawn.cpp` |  | role of the test's own copy spawned as a `posix_spawn` child, which exits or sleeps on it and otherwise fails |
| `ANYPS5_TEST_QUOTED_ARGV0` | presence | `core/libs/tests/GuestLoadExecHost.cpp` |  | makes the load-exec test's first argument one with quotes, a backslash and non-ASCII text |
| `APS5_EBOOT` | string | `core/libs/prx/libSceSystemService/Export.cpp` |  | host path of the executable launched for /app0/eboot.bin; unset or empty means the running executable |
| `APS5_LOAD_EXEC_STANDARD_ERROR` | string | `core/libs/tests/GuestSystemServiceLoadExecStandardError.cpp` |  | marker that the standard-error load-exec test sets in its child process to choose a mode, `list` or `none`; not a product setting |
| `APS5_SKIP_TIMING_TESTS` | string | `core/libs/tests/PreciseWait.cpp` |  | treats the timing tests' runner as preempted, so they skip without probing the clock; an empty value or one starting with 0 leaves the probe on |
| `APS5_USBD_DEVICE` | string | `core/libs/tests/GuestUsbd.cpp` |  | selects the USB device a test drives, as vendor:product in hex |

### core/relinker

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_GUEST_MODULE_ARGS` | string | `core/relinker/elfpatcher/tests/GuestModuleArgsHost.cpp` |  | path of the file the module-argument fixture appends its events to |

### core/shader

`APS5_LOOP_GUARD=<exits>` limits evaluated loop exits per shader invocation (`core/shader/recompiler/SpirvBackend/src/SpirvEmitter.cpp`); `0` or an unset variable disables that limit. Its presence separately enables the AGC fault buffer, as listed under `libSceAgcDriver`.

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_NO_SHADER_CACHE` | string | `core/shader/recompiler/ShaderDiskCache.cpp` |  | disables the on-disk shader cache when set to a non-empty value other than 0 |
| `ANYPS5_SHADER_CACHE_DIR` | string | `core/shader/recompiler/ShaderDiskCache.cpp` |  | directory for the on-disk shader cache, overriding the one beside the executable |
| `APS5_BDA_BYTE_READS` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvBdaRead.cpp` |  | forces one address lookup and byte load per byte for BDA reads instead of a wide dword load |
| `APS5_BDA_DWORD_STORES` | presence | `core/shader/recompiler/Translation/src/MemoryInstructions/FlatInstructions.cpp` |  | reverts BDA writes to one address store per dword instead of a wide multi-dword store |
| `APS5_DUMP_IR` | string | `core/shader/recompiler/Recompiler.cpp` |  | hex code address (or all) of the programs whose IR is printed after each front-end pass |
| `APS5_EXACT_RCP` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvAlu/SpirvAluEmitterReciprocal.cpp` |  | emits `v_rcp_f32` and `v_rcp_iflag_f32` as the RDNA hardware reciprocal (bit-exact for normal inputs and results) instead of the driver's division; adds about 13k SPIR-V words to each shader that uses them |
| `APS5_GATHER_CONST` | integer | `core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp` |  | integer every non-depth gather returns in all four texels instead of the sampled value |
| `APS5_IMAGE_TABLE_KEY_SCAN` | integer | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` | 256 | number of image-table entries the key scan examines |
| `APS5_IMAGE_TABLE_STRICT` | string | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` |  | sets image-table materialization to its strict form; 0 leaves it off |
| `APS5_NO_ATOMIC_BARRIER` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvMemory/SpirvFormatConvert.cpp` |  | drops the device-scope acquire/release memory barrier emitted after atomics |
| `APS5_NO_ATOMIC_ZERO_SKIP` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvMemory/SpirvMemoryInstructions.cpp` |  | restores plain buffer atomics instead of skipping identity-operand atomics |
| `APS5_NO_BUFFER_SUBSTITUTES` | presence | `core/shader/recompiler/Recompiler.cpp` |  | specializes a null buffer V# with its own words instead of the fields its slot had the first time a dispatch of the same program bound it |
| `APS5_NO_CODE_HASH_KEY` | presence | `core/shader/recompiler/CacheKey.hpp` |  | appends every code word to the cache key instead of hashing the code |
| `APS5_NO_FAILURE_MEMO` | presence | `core/shader/recompiler/Recompiler.cpp` |  | rebuilds a program whose compilation failed instead of reusing the memoized failure |
| `APS5_NO_IMAGE_TABLES` | presence | `core/shader/recompiler/Optimization/src/ResourceTracker.cpp` |  | disables image tables, leaving table-loaded descriptors unplanned |
| `APS5_NO_MASKED_RESULT_MEMO` | presence | `core/shader/recompiler/Recompiler.cpp` |  | materializes every result memo miss instead of patching the buffer descriptors, user data and flattened SRT of a cached result whose snapshot differs only in those words |
| `APS5_NO_PERF_DISK_PLANS` | presence | `core/shader/recompiler/Recompiler.cpp` |  | disables the disk cache of planning results |
| `APS5_NO_PERF_DOMINATOR_TREE` | presence | `core/shader/recompiler/ControlFlow/src/Structurizer.cpp` |  | computes the structurizer's dominator and post-dominator sets with the set fixpoint instead of a dominator tree, walks every selection construct instead of testing its members by dominance, and tests every block against each construct when checking the structured constructs instead of walking the construct |
| `APS5_NO_PERF_DOM_INTERSECTION` | presence | `core/shader/recompiler/ControlFlow/src/Structurizer.cpp` |  | computes dominator intersections into a fresh vector instead of intersecting in place |
| `APS5_NO_PERF_ITERATION_SELECTION` | presence | `core/shader/recompiler/ControlFlow/src/Structurizer.cpp` |  | recomputes the post-dominator sets when finding a loop's iteration-selection merge instead of reusing them |
| `APS5_NO_RESULT_MEMO` | presence | `core/shader/recompiler/Recompiler.cpp` |  | materializes a fresh result for every recompile instead of reusing the result memo |
| `APS5_NO_SRT_TAPE` | presence | `core/shader/recompiler/Optimization/src/SrtWalker/SrtTape.cpp` |  | runs the runtime-source walk of a plan in the IR evaluator instead of the compiled tape |
| `APS5_NO_TABLE_RESOLUTION_REUSE` | presence | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` |  | stops a result memo miss from reusing an unchanged image table's resolution, so the table is resolved again |
| `APS5_NO_TABLE_SNAPSHOT_REUSE` | presence | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` |  | turns off the image table snapshot reuse, so every capture reads and compares each table column |
| `APS5_NO_WINDOW_NARROWING` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvMemory/SpirvModuleSetup.cpp` |  | indexes every slot of the shader's vector window instead of only the slots the program uses |
| `APS5_PROBE` | string | `core/shader/recompiler/Translation/src/DispatchInstructions.cpp` |  | pc:vgpr[:shift] probe that copies that VGPR into v255 after the instruction so image stores write it as a per-lane trace |
| `APS5_RAYTRACING` | string | `core/shader/recompiler/Translation/src/DispatchInstructions.cpp` |  | selects the image_bvh_intersect_ray behaviour: strict throws and miss writes 0xffffffff |
| `APS5_RESULT_MEMO_MB` | integer | `core/shader/recompiler/Recompiler.cpp` | 256 | MiB the result memos of all shader sources may hold before the least recently used results are dropped (0 keeps them until their set evicts them) |
| `APS5_SAVEEXEC_WRITE_FIRST` | presence | `core/shader/recompiler/Translation/src/ControlFlowInstructions.cpp` |  | writes the saved exec to the destination before reading the source in s_*_saveexec |
| `APS5_SINGLE_LANE` | string | `core/shader/recompiler/Recompiler.cpp` |  | hex code addresses (comma separated, or all) of wave64 programs kept at one guest lane per invocation |
| `APS5_SRT_DEBUG` | presence | `core/shader/recompiler/Optimization/src/RequestMemoryView.cpp` |  | prints extra SRT descriptor-walk debug output, such as guest reads outside the request memory |
| `APS5_STRICT_DESCRIPTOR_WEBS` | string | `core/shader/recompiler/Optimization/src/ResourceTracker.cpp` |  | makes a T# or S# read in a descriptor web throw when a path's descriptor dwords depend on the program counter, EXEC or a lane mask, instead of selecting the first descriptor and recording an `InvalidDescriptor` fault when a lane takes that path; 0 leaves it off |
| `APS5_TRACE_BDA` | presence | `core/shader/recompiler/Optimization/src/ResourceTracker.cpp` |  | prints the first few accesses that make a program take the address-based path |
| `APS5_TRACE_BUFFER_WRITTEN` | presence | `core/shader/recompiler/Optimization/src/DescriptorBindingBuilder.cpp` |  | prints how many guest buffer elements a program may store to and how many it only loads |
| `APS5_TRACE_IMAGE_TABLE` | presence | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` |  | traces image-table resolution |
| `APS5_VERIFY_MASKED_RESULT_MEMO` | presence | `core/shader/recompiler/Recompiler.cpp` |  | also materializes every patched result memo hit, prints the first twenty whose patched result differs and counts them in the result memo report of `APS5_PROFILE_DRAW` |
| `APS5_VERIFY_SRT_COMPACT` | presence | `core/shader/recompiler/Optimization/src/SrtWalker/SrtDescriptorEvaluation.cpp` |  | walks every SRT descriptor a second time through the IR evaluator and throws when the two walks differ |
| `APS5_VERIFY_SRT_TAPE` | presence | `core/shader/recompiler/Optimization/src/SrtWalker/SrtTape.cpp` |  | walks a plan's runtime sources a second time through the evaluator, adopts its answers where the two differ and prints the first twenty differences |
| `APS5_WAVE_LDS_SUBGROUP` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvEmitter.cpp` |  | restores subgroup-scope LDS barriers for single-wave workgroups instead of workgroup scope |

### libSceAgc

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_TRACE_LABEL` | integer | `core/libs/prx/libSceAgc/Cb/src/Sync.cpp`, `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/LabelHistory.cpp` | 0 | hex guest address; logs a matching RELEASE_MEM when recorded, and nearby RELEASE_MEM, WRITE_DATA, COPY_DATA, DMA_DATA and WAIT_REG_MEM packets when executed |

### libSceAgcDriver

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_GPU` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | restricts physical device selection to device names containing this case-insensitive substring |
| `ANYPS5_HOST_REPORT` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 0 | prints the host report to stderr once when the presenting device is chosen: 1 prints it; 0, empty or unset prints nothing, and any other value stops the start with an error naming the variable |
| `ANYPS5_OVERLAY` | string | `core/libs/prx/libSceAgcDriver/Execution/src/PerformanceOverlay.cpp` | 0 | the performance overlay's page at start: 1 the overlay, 2 its detailed page; 0 or unset hides it, and any other value is refused |
| `ANYPS5_OVERLAY_POS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/PerformanceOverlay.cpp` | tl | the overlay's corner: tl, tr, bl or br |
| `ANYPS5_PAD_STATUS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/PadStatusOverlay.cpp` | 0 | 1 shows the controller status panel for 4 seconds when a controller connects, disconnects or enters a lower battery class; 0 or unset never shows it on its own (the `ShowPadStatus` key still does), and any other value is refused |
| `ANYPS5_PRESENT_MODE` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | fifo | selects the swapchain present mode: fifo, fifo_latest_ready, mailbox or immediate |
| `ANYPS5_REQUIRE_DESCRIPTOR_BINDING_FLAGS` | presence | `core/libs/prx/libSceAgcDriver/tests/ComputePipelineReplay.cpp` |  | fails the replay test instead of accepting a device that refuses descriptor binding flags |
| `ANYPS5_REQUIRE_ROBUST2` | presence | `core/libs/prx/libSceAgcDriver/tests/execution/LdsMixedWidthAtomics.cpp` |  | fails the LDS lock-budget test when the device does not enable robustBufferAccess2, instead of only printing it |
| `ANYPS5_REQUIRE_VULKAN` | presence | `core/libs/prx/libSceAgcDriver/tests/BdaDevice.cpp` |  | fails a test instead of skipping when no usable Vulkan device is present |
| `ANYPS5_SCREENSHOTS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/Screenshot.cpp` |  | directory the screenshot key saves its PNG files to, defaulting to anyps5-screenshots beside the executable |
| `APS5_AB` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/RuntimeSwitches.cpp` |  | `<seconds>:<switch>[+<switch>...]`: alternates phase A (switches as the environment sets them) and phase B (the listed runtime switches toggled) at flip packets, each phase at least `<seconds>` long; prints `[ab]` lines; a name that is not a runtime switch fails the first flip |
| `APS5_AB_SETTLE` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/RuntimeSwitches.cpp` | 2 | seconds into a phase after which APS5_AB prints the phase's `settled` line; shorter than the phase |
| `APS5_AB_START` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/RuntimeSwitches.cpp` | 0 | flip packet count (from 1) at which APS5_AB starts, or `<n>s` for seconds after the first flip |
| `APS5_ADDRESS_COPY_MAX_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 64 | upper bound in MiB on the guest memory one address-based build copies; exceeding it aborts |
| `APS5_ALL_BUFFERS_WRITTEN` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | notes every shader buffer element as written, as before bufferWritten existed |
| `APS5_ASSERT_GPU_LOCK` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | aborts when a guarded path is reached without holding GuestMemory::GpuMutex |
| `APS5_ATOMIC_STAGE_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 1024 | size cap in KiB on an atomic shader element covered by device-local staging |
| `APS5_BACKGROUND_COMPILE_KIB` | integer | `core/libs/prx/libSceAgcDriver/Execution/include/CompileBudget.hpp` | 8192 | KiB of SPIR-V that compute pipelines built ahead of their dispatch (dispatch prebuilds and the pipeline journal's replay) may be compiling at once; a build that does not fit waits, one larger than the limit builds alone, and 0 builds them one at a time. Dispatches that need a pipeline now are not limited |
| `APS5_BARRIER_VALIDATE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | counts the barriers a hazard-tracking validator would emit and skip, by class and hazard kind |
| `APS5_BATCH_CAP` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` | 64 | recorded work since the last submit at which a label batch, flush or reap is forced (0 disables the cap) |
| `APS5_BDA_FLUSH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | also flushes pending storage results over address-based shader regions, which are otherwise exempt |
| `APS5_BDA_STAGING_MIN_THREADS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 512 | smallest dispatch, in threads, whose address-based build stages its written elements device-local; an indirect dispatch always does |
| `APS5_BDA_TABLE_CACHE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/BdaResources.cpp` | 8 | number of recently used BDA page tables kept in the cache, clamped to 1..64 (1 is the previous behaviour) |
| `APS5_BLOCK_WRITEBACK_EACH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | stores the touched units only on every write-back instead of widening repeated partial stores |
| `APS5_BLOCK_WRITEBACK_FRAMES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` | 8 | window of presents within which repeated partial stores trigger the whole-image widening |
| `APS5_BLOCK_WRITEBACK_PIECES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` | 4 | number of partial stores within the window that triggers the whole-image widening |
| `APS5_BLOCK_WRITEBACK_WIDEN` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | enables the widening that stores a whole image when it keeps being asked for in pieces |
| `APS5_BUFFER_POOL_SHARED` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/BufferPool.cpp` |  | uses one buffer tier for every allocation size, as before the tier split |
| `APS5_BUFFER_POOL_SLOTS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/BufferPool.cpp` | 4096 | bounds the buffers each pool tier retains (0 keeps the default of 4096) |
| `APS5_CAPTURE_GPU_COPIES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | records GPU buffer copies in the capture trace (requires APS5_CAPTURE_TRACE) |
| `APS5_CAPTURE_INPUTS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | records each draw's inputs in the capture trace (requires APS5_CAPTURE_TRACE) |
| `APS5_CAPTURE_TRACE` | presence | `core/libs/prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp` |  | writes capture events to capture-trace.log |
| `APS5_CAPTURE_TRACE_PATH` | string | `core/libs/prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp` | capture-trace.log | file path for the capture events log |
| `APS5_CHECK_INDIRECT_ARGS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | reads GPU-side indirect draw records back on the CPU and prints the first ones |
| `APS5_COMPLETION_STORE_SYNC` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | makes a store made by a completion action wait for later batches over its range |
| `APS5_COPY_DESTINATION_STRICT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | refuses a CPU copy whose destination has any pending writer, not only an unsettled one |
| `APS5_COPY_HLE_CPU_MAX` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` | 262144 | largest copy in bytes the CPU side of the copy HLE handles |
| `APS5_COPY_HLE_GPU_MAX` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` |  | largest copy in bytes the GPU side of the copy HLE handles; unset means no limit |
| `APS5_COPY_READ_TRACKING` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | notes in-place reads of a range; set to 0 to disable, making the CPU copy path require an idle recorder |
| `APS5_COPY_SYNC` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | treats any pending writer over a copy's range as needing a wait |
| `APS5_COPY_VERIFY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | verifies a CPU copy's source and destination bytes against the pre-copy values after a sync |
| `APS5_COPY_WAIT_SOURCE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | waits for the source's producer batch when it is the only one in flight, on non-zero queues |
| `APS5_COUNT_ALL_COMPLETION_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | counts a completion label also stored on the GPU as pending from its registration |
| `APS5_CPU_COPIES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | copies misaligned descriptor sub-ranges on the CPU instead of with a GPU buffer copy |
| `APS5_CPU_DCC_KEYS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/DccMetadata.cpp` |  | always stores DCC keys on the CPU |
| `APS5_CPU_FLIP_IN_ORDER` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/Submission.cpp` |  | orders every CPU flip behind queued work instead of presenting unqueued flips at once |
| `APS5_CPU_STORES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketMemory.cpp` |  | writes packet stores on the CPU instead of recording them on the GPU |
| `APS5_DCC_KEYS_EACH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records every DCC key store at once instead of batching them |
| `APS5_DEPTH_COPY_MIN_KIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 1024 | minimum surface size in KiB for a depth target to take the depth-copy path |
| `APS5_DEPTH_PLANE_COPY_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp` | 256 | budget in MiB of the sampled depth-plane copies kept for reuse (least recently used copies leave first; copies sampled since the previous present stay while all copies hold at most twice the budget); `0` keeps every copy |
| `APS5_DEPTH_PLANE_WHOLE_STORE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp` |  | stores a stale depth plane whole when memory over it is read, instead of only the 64 KiB units the read overlaps |
| `APS5_DISPATCH_CACHE_BYTES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` | 268435456 | size of the dispatch cache in bytes (0 keeps the default of 256 MiB) |
| `APS5_DISPATCH_CACHE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` | 8192 | number of dispatch cache entries; 0 picks a default from the variant count: 8192 with the default 4 variants, 16384 with 2 or fewer, 4096 under `APS5_DISPATCH_STAMP_VALIDATE` |
| `APS5_DISPATCH_STAMP_VALIDATE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` |  | validates dispatch cache hits by write stamps, forcing one variant and a 4096-entry cache |
| `APS5_DISPATCH_VARIANTS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` | 4 | number of dispatch variants kept per cache entry, clamped to the maximum |
| `APS5_DISPMOVE_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 1024 | upper bound in KiB on the read-only buffers a dispatch-cache hit moves into its template, above which the dispatch is prepared again |
| `APS5_DRAIN_ALL` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketMemory.cpp` |  | drains every packet, disabling the batched recording path |
| `APS5_DRAIN_COMPLETION_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | records completion labels immediately instead of deferring them with the batch, and drains a label recorded while a copied write-back is pending instead of storing it behind the write-backs |
| `APS5_DRAW_CACHE_BYTES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` | 268435456 | size of the draw cache in bytes, evicting least-recently-used entries past it (0 keeps the default of 256 MiB) |
| `APS5_DRAW_CACHE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` | 4096 | number of entries in the draw cache; 0 keeps the default 4096 |
| `APS5_DRAW_CACHE_INDIRECT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` |  | also caches the draws an indirect packet runs on the CPU path (records read on the host, patched user words); a hit still recompiles the stages whose user words the record patches and reuses the other stages |
| `APS5_DRAW_DRAIN` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketMemory.cpp` |  | treats draw packets as drains, waiting for them instead of recording ahead |
| `APS5_DRAW_INPUTS_IN_PLACE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | binds draw vertex and index ranges in place from host imports also with NVIDIA's Windows driver, which copies them by default |
| `APS5_DRAW_KEY_ALL_USER_WORDS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawRegisterKey.cpp` |  | includes all 32 user words in a draw's register key instead of the used count |
| `APS5_DRAW_PIPELINE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | runs the second half of a pipelined draw on a draw finisher thread and keeps proofs for the collect epoch |
| `APS5_DRAW_SNAPSHOT_MAX_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 64 | upper bound in MiB on the snapshot bytes a draw's bindings may take |
| `APS5_DRAW_TRANSITIONS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | keeps the per-draw upload and download barriers and layout transitions instead of the lean recorded path |
| `APS5_DRAW_UPLOAD_MAX` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 4096 | largest draw snapshot in bytes copied into the per-recorder upload chunk; 0 turns the chunk off |
| `APS5_DUMP_COMPUTE_MIN_WORDS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | only captures compute pipelines whose SPIR-V has at least this many words (an integer from 0 through 8388608; any other value fails) |
| `APS5_DUMP_COMPUTE_PIPELINES` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | directory in which captured compute pipelines are written |
| `APS5_DUMP_DRAW_SHADERS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` | 0 | hex color-target or slot-0 address, or a program's code address, whose draw's shaders are dumped |
| `APS5_DUMP_DRAW_SLOT1` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` | 0 | hex slot-1 address whose draw's shaders and register state are dumped |
| `APS5_DUMP_FRAMES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 0 | saves the first n presented frames as frame_<index>.bmp |
| `APS5_DUMP_FRAMES_EVERY` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 1 | dumps every n-th presented frame |
| `APS5_DUMP_QUEUE` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | dumps the first submissions of the queue with this hex id |
| `APS5_DUMP_REJECTED` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/CommandValidation.cpp` |  | writes a rejected submission to rejected_submission_<n>.bin |
| `APS5_DUMP_SCALE` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 4 | keeps every n-th pixel in each direction of a dumped frame (1 to 16384; any other value fails) |
| `APS5_DUMP_SHADERS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | writes each prepared shader's request to shader_<address>.req; a comma-separated list of hex program addresses (for example `807b90a100,0x807b925400`) writes only those programs' requests, and any other value (such as `1`) writes every request |
| `APS5_DUMP_STORAGE` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | selects storage textures to dump by hex address or WxHxL[:format] extent as storage_<address>_<n>.raw |
| `APS5_DUMP_STORAGE_GATE` | path | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` | | with `APS5_DUMP_STORAGE`, waits for this path to exist and reads matching live images directly without guest writeback; at most eight addresses, 8 MiB linear storage per image; writes `storage_direct_<address>_<n>.raw` with the existing pitch, height and VkFormat header and first-mip pixels |
| `APS5_TEST_DIRECT_READBACK` | presence | `core/libs/prx/libSceAgcDriver/tests/execution/LayeredVolumeDraw.cpp` | | runs the clean-volume gated live-image readback regression in the layered volume execution test |
| `APS5_DUMP_SUBMITS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/SubmissionCapture.cpp` |  | directory in which every submission is captured for replay |
| `APS5_DUMP_TARGETS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` | 0 | dumps the first n renders of each non-resident color target as target_<address>_<n>.raw in the working directory; a non-zero value also runs every draw synchronously, and in practice it needs APS5_NO_RESIDENT_TARGETS=1 |
| `APS5_DUMP_TEXTURE` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | hex addresses or WxH extents whose sampled textures are dumped on each of their first 8 uploads as texture_<address>_<n>.raw |
| `APS5_EAGER_WRITEBACK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | writes back pending storage layers eagerly |
| `APS5_EXACT_DRAW_WRITES` | string | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WrittenBuffers.cpp` |  | sizes a color or depth target's draw write ranges from its own bytes and slices (the default when unset); `0` uses the slack and unknown-byte approximation instead |
| `APS5_FILL_CHAIN` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | seeds and doubles the fill pattern in place in the import |
| `APS5_FILL_CLEAR_EXACT_ONLY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferFill.cpp` |  | turns a fill into a clear only when it covers the target exactly and nothing else |
| `APS5_FILL_SYNC` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | waits for every recorded store over the fill's range |
| `APS5_FLIP_INFLIGHT` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 1 | number of presentations that may trail on the GPU (0, 1 or 2) |
| `APS5_FLIP_READ_CHECK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records the in-place reads of completed batches and checks them before the present blit's fence |
| `APS5_FORCE_SDR` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/HostHdr.cpp` | unset | suppresses advertised HDR availability while retaining detected host capabilities; uses the existing SDR presentation path |
| `APS5_FORCE_STRUCTURIZER_FALLBACK` | presence | `core/shader/recompiler/ControlFlow/src/Structurizer.cpp` |  | makes the structurizer use the reach-flag fallback for every graph that supports it instead of trying cloning first; the control-flow tests then expect that fallback |
| `APS5_FRAME_CAPTURE` | string | `core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp` |  | directory in which the driver records a few frames of the title's driver calls and guest memory for `agc_frame_replay` |
| `APS5_FRAME_CAPTURE_AT_FLIP` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp` | 0 | frame boundary (CPU flip or submission that flips) at which the frame capture starts when no trigger file is set; 0 and 1 both start at the first boundary; a value that is not a number fails |
| `APS5_FRAME_CAPTURE_FRAMES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp` | 10 | number of frame boundaries the frame capture records |
| `APS5_FRAME_CAPTURE_MIN_FREE_BYTES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp` | 4294967296 | free bytes the capture volume must keep or the frame capture stops with a warning; the tests set 0 so a nearly full runner disk cannot fail them |
| `APS5_FRAME_CAPTURE_TRIGGER` | string | `core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp` |  | file whose appearance starts the frame capture at the next frame boundary; a number in it replaces the frame count |
| `APS5_FULL_BARRIERS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records every barrier instead of merging redundant ones |
| `APS5_GPU_CHECKPOINTS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | attaches device diagnostic checkpoints through VK_NV_device_diagnostic_checkpoints, or buffer markers through VK_AMD_buffer_marker on a device without it |
| `APS5_GPU_COPY_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 1024 | size cap in KiB on a descriptor sub-range copied through the GPU path, larger ones staying on the CPU |
| `APS5_GPU_NO_WRITEBACK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | skips writing GPU results back into guest memory |
| `APS5_GPU_WAIT_TIMEOUT_MS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` | 1000 | milliseconds a GPU wait may run before a warning is printed |
| `APS5_GROUP_CAPTURE_ALL_QUEUES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchAhead.cpp` |  | resolves group-ahead capture on queue 0 as well |
| `APS5_HANDOFF_SPIN_US` | integer | `core/libs/prx/libSceAgcDriver/Execution/include/HandoffSpin.hpp` |  | microseconds a thread spins for a handed-off draw's work or for the GPU mutex before blocking, clamped to 0..10000; unset spins 150 us when the process has more than 8 usable processors and not at all otherwise |
| `APS5_HEAP_MIRROR_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 24576 | budget in MiB for the heap mirrors held for registered allocations |
| `APS5_HOOK_FLUSH_CPU_BLOCKS` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | set to 0 flushes pending images whose every 64 KiB block the CPU wrote, instead of skipping them |
| `APS5_HOOK_FULL_SYNC` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | makes the flush hook's recorded-store wait a full Sync under the GpuMutex |
| `APS5_HOOK_LOCKED_WAIT` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | keeps the flush hook's pending-write wait under the GpuMutex |
| `APS5_HOST_IMPORT_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 0 | budget in MiB for pinned host imports; `0` selects the larger of 6144 MiB and half the physical memory, at most the largest host heap minus the larger of 2 GiB and half that heap (4055 MiB on a 16 GB Windows PC with an RTX 4060) |
| `APS5_IGNORE_DEPTH_TEST` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp` |  | renders depth- and stencil-tested draws without a depth target as if the tests always passed |
| `APS5_IMAGE_POOL_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/BufferPool.cpp` |  | size in MiB of the released image memory the buffer pool keeps for reuse by memory type and size while the device-local heap is under its reported budget, a quarter of the device tier's budget by default; `0` frees image memory at once |
| `APS5_INDIRECT_VERTEX_MIB` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/IndirectDraw.cpp` | 64 | cap in MiB on a vertex attribute extent, above which a GPU-side indirect draw resolves on the CPU |
| `APS5_INSERT_COMPARE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` |  | compares a fresh capture's regions against the recorder before inserting it into the cache |
| `APS5_KEPT_DESCRIPTOR_SETS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 8 | number of freed dedicated image table descriptor sets kept for a later build with the same layout and sizes; 0 keeps none |
| `APS5_KEYFLIP_DROP` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | drops a pending unit's results on a clear-code to uncompressed key flip, as before |
| `APS5_KEYS_FILL_CLEAR` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | clears an image from DCC keys whose fill pattern it matches (only when set to 1) |
| `APS5_LABEL_FLUSH_US` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | age in microseconds at which a pending label batch is recorded or submitted; unset is 1000 with batched label submission and 250 when `APS5_NO_LABEL_BATCH_SUBMIT` or `APS5_NO_LABEL_BATCHING` turns it off |
| `APS5_LABEL_GATE_ALL` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | gates a label store on any pending completion, not only one overlapping the range |
| `APS5_LABEL_LOCK_EACH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | records each label under the GpuMutex instead of deferring it |
| `APS5_LABEL_OWN_LOCK` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketFlush.cpp` |  | makes a label flush take the GpuMutex itself instead of relying on a self-locking packet |
| `APS5_LABEL_REAP_ALL_QUEUES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | reaps completion labels on every queue instead of only queue 0 |
| `APS5_LABEL_RUNS_INLINE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records store runs at once and closes each run at the next command |
| `APS5_LABEL_STORE_ALWAYS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | writes back every label store even when a later store already covered its bytes |
| `APS5_LABEL_SUBMIT_NOW` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | submits every label immediately instead of waiting for the flush deadline |
| `APS5_LABEL_TRUST_LATE` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | trusts cache entries with a stamp at or below the wait's stamp under the late rule (off unless set to a non-zero value) |
| `APS5_LABEL_TRY_EACH_PACKET` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | attempts to record deferred labels at every self-locking packet instead of deferring to its lock |
| `APS5_LAND_WAITS_ALL_QUEUES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | notes pending waits on queue 0 as well |
| `APS5_LAZY_SHADER_PREPARE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | prepares a registered shader on its first use instead of while registering it |
| `APS5_LDS_SLACK` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderInputState.cpp` | 0 | dwords added to every dispatch's LDS allocation |
| `APS5_LOCKED_BUILD` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | builds the whole dispatch preparation under the GpuMutex, skipping the split path |
| `APS5_LOCKED_DRAW_PREPARE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` |  | takes the GpuMutex before preparing a draw |
| `APS5_LOG_STATS` | string | `core/libs/prx/libSceAgcDriver/Execution/include/AsyncLog.hpp` |  | prints the AGC log's line, byte and coalescing statistics every 10 s window by default, when unset or set to anything but `0`; `0` turns them off |
| `APS5_LOG_SYNC` | presence | `core/libs/prx/libSceAgcDriver/Execution/include/AsyncLog.hpp` |  | writes AGC log lines on the calling thread instead of through the asynchronous sink thread |
| `APS5_LOOP_GUARD` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | enables the fault buffer that guards shader loops running past their bound |
| `APS5_MIRROR_REFRESH_THREADS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 3 | number of helper threads that refresh heap mirrors, clamped to 0 through 16 |
| `APS5_NO_ADAPTIVE_STAGING` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | stages every element the staging window admits instead of probing each busy element by frame GPU time and keeping it in place when that is faster |
| `APS5_NO_ADDRESS_READ_PASSES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | stops a draw that only reads through its buffer descriptors from counting as read-only, so it breaks the pass instead of continuing the producer's |
| `APS5_NO_ADDRESS_SPACE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | disables the address-space cache so every build rebuilds its space |
| `APS5_NO_ADJACENT_GENERATION` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | leaves adjacent pending images at their generation during a write-back, as before |
| `APS5_NO_ALIAS_BORROW` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | disables borrowing a pending aliased storage image |
| `APS5_NO_ATOMIC_STAGING` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | binds atomic shader elements in place instead of staging them through a device-local buffer |
| `APS5_NO_BARRIER_BATCH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/RuntimeSwitches.cpp` |  | records the staging, write-back and label-run leading barriers that a covering barrier already orders, instead of leaving them out; APS5_AB can toggle it |
| `APS5_NO_BARRIER_ELISION` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records every leading barrier instead of merging it into the following submission |
| `APS5_NO_BDA_STAGING` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | binds the written elements of address-based compute builds in place in the host imports instead of staging them device-local |
| `APS5_NO_BDA_TABLE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/BdaResources.cpp` |  | builds every BDA descriptor table instead of reusing the recent-table cache |
| `APS5_NO_BLOCK_TRACKING` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | tracks array layers, or the whole surface, instead of 64 KiB blocks |
| `APS5_NO_BUFFER_CLASSES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/BufferPool.cpp` |  | matches exact sizes in the buffer pool instead of rounding up to size classes |
| `APS5_NO_CACHED_PRECOLLECT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | leaves the write-watch walks of a cached object's Revalidate under the GPU mutex |
| `APS5_NO_CLEAR_HAND_OVER` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | writes other images' pending results back at a clear-key storage refresh instead of handing them over to the refreshed image |
| `APS5_NO_CLEARED_VIEW` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | keeps fast-cleared surfaces as CPU snapshots instead of viewing the cleared storage image |
| `APS5_NO_COLLECT_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | disables the per-thread memo of write-watch collects |
| `APS5_NO_CONTEXT_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | rebuilds the Graphics::Context on every call instead of returning the cached one |
| `APS5_NO_COPY_ALIAS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | keeps every device copy a transfer instead of aliasing the source storage image |
| `APS5_NO_COPY_HLE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` |  | disables the copy-kernel HLE, so matching copies run as compute |
| `APS5_NO_COPY_KNOWN_VALUES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` |  | disables serving a copy's destination from source values the driver already knows |
| `APS5_NO_CPU_COPY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` |  | disables the CPU copy path of the copy HLE |
| `APS5_NO_DATA_HITS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` |  | disables dispatch-cache hits that differ only in ShaderData or FlattenedSrt words |
| `APS5_NO_DCC_PRESYNC` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | leaves the DCC key ranges out of the surfaces a build pre-syncs, so only the surface itself waits for its producer |
| `APS5_NO_DEFERRED_DESTRUCTION` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | on MoltenVK, destroys buffers and frees device memory as soon as the driver releases them, instead of while the graphics queue is idle (KhronosGroup/MoltenVK#2876) |
| `APS5_NO_DCC_WORD_SCAN` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/DccMetadata.cpp` |  | restores the byte-by-byte DCC key scan instead of the word-at-a-time compare |
| `APS5_NO_DEFERRED_EVICTION` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | destroys displaced resource-cache entries under the mutex instead of deferring them to the open batch |
| `APS5_NO_DEFERRED_SETTLES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | settles watched private host imports as they open instead of through the deferred block-settling path (Windows only) |
| `APS5_NO_DIRECT_DEPTH_COPY` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp` |  | keeps every copy between a depth plane and a color storage image on the staging buffer instead of one `vkCmdCopyImage` (used when `VK_KHR_maintenance8` is enabled and the storage format is a single component of the aspect's size) |
| `APS5_NO_DISPATCH_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | disables the dispatch cache so every dispatch takes the uncached path |
| `APS5_NO_DISPATCH_KEY_HYGIENE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | mixes every shader register into the dispatch key instead of only the registers that affect the dispatch |
| `APS5_NO_DISPATCH_LRU` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchInsert.cpp` |  | clears the whole dispatch cache on overflow instead of evicting the least recently used entry |
| `APS5_NO_DISPATCH_MOVE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | leaves a dispatch-cache hit from moving its read-only buffers into the template, so it prepares the dispatch again; also off when APS5_NO_TEMPLATE_DATA_REFRESH is set |
| `APS5_NO_DISPATCH_RECIPE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | builds no dispatch recipes, so dispatch-cache hits repeat the recorded work |
| `APS5_NO_DRAW_DATA_HITS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` |  | disables draw-cache hits that differ only in ShaderData words |
| `APS5_NO_DRAW_INPUTS_IN_PLACE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | copies draw vertex and index ranges instead of binding them in place from host imports, on any device; wins over `APS5_DRAW_INPUTS_IN_PLACE` |
| `APS5_NO_DRAW_KEY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` |  | registers no draw key, so every draw decodes its programs instead of hitting the draw cache |
| `APS5_NO_DRAW_KEY_TRIM` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | keys the render target and index ranges into the draw resource key instead of repeating the alias checks on a hit |
| `APS5_NO_DRAW_PRECHECK` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawPrecheck.cpp` |  | disables the draw precheck that rejects empty or invalid draws early |
| `APS5_NO_DRAW_RECIPE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | builds no draw recipes, so draw-cache hits repeat the recorded stages |
| `APS5_NO_DRAW_RELOCATION` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawRelocate.cpp` |  | disables draw relocation, which rewrites a draw's moved user words from a learned variant instead of preparing the draw again |
| `APS5_NO_DRAW_RESOURCE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | builds every draw's shader resources instead of reusing a cached identical build |
| `APS5_NO_DRAW_SRT_ENTRIES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` |  | disables the draw cache's SRT entries, so every draw decodes its programs |
| `APS5_NO_DRAW_STAGE_DATA_HITS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` |  | disables draw-cache hits that differ only in a stage's ShaderData words |
| `APS5_NO_EMPTY_INDIRECT_SKIP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` |  | builds the stages of an indirect draw whose front program last took the CPU path even when none of its records draws anything, instead of reading its records first and skipping it |
| `APS5_NO_EPOCH_COLLECT_MAP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | drops the per-epoch map of completed collects, leaving only the fixed-size thread memo to answer a repeated collect |
| `APS5_NO_EPOCH_REVALIDATE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | runs every Revalidate's per-element and per-region checks instead of the epoch gate |
| `APS5_NO_EXACT_ROBUST_LOADS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | keeps the descriptor-range comparison in direct buffer loads on a device that zeroes past the range |
| `APS5_NO_FAST_REVALIDATE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | always repeats the resource lookups instead of using the fast proof |
| `APS5_NO_FILL_CLEAR` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferFill.cpp` |  | disables the clear-through-DCC-keys path in a fill |
| `APS5_NO_FILL_HLE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferFill.cpp` |  | disables the fill-kernel HLE, so matching fills run as compute |
| `APS5_NO_FLUSH_PRETEST` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | lists every pending image on FlushPending instead of testing whether a pending unit lies inside the range |
| `APS5_NO_FOREIGN_WRITERS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | records no draw-written ranges as foreign writers for capture validation |
| `APS5_NO_GATE_RETRY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | retries no validation whose entry moved, leaving the dispatch or draw unvalidated |
| `APS5_NO_GPL` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | draws without graphics pipeline libraries; the device still enables their extensions, so the driver builds its pipelines as for a default device |
| `APS5_NO_GPU_DUMP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | decodes dump frames from the tiled guest buffer on the CPU |
| `APS5_NO_GPU_INDIRECT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/IndirectDispatch.cpp` |  | reads indirect dispatch arguments on the CPU instead of letting the GPU count them |
| `APS5_NO_GPU_INDIRECT_DRAW` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | reads indirect draw arguments through the flush hook on the CPU instead of from the GPU |
| `APS5_NO_GPU_SAMPLE_DUMPS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | dumps occlusion-counter samples on the CPU instead of through the GPU copy path |
| `APS5_NO_GPU_THREAD_DIMENSIONS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/IndirectDispatch.cpp` |  | reads the thread counts of an indirect dispatch with USE_THREAD_DIMENSIONS on the CPU instead of turning them into group counts and a thread limit on the GPU |
| `APS5_NO_HOLD_PROFILE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | turns off GpuMutex hold-time profiling, keeping only the wait timing |
| `APS5_NO_HOOKSYNC_PROFILE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | leaves only the plain hook-sync counts under APS5_PROFILE_DRAW |
| `APS5_NO_HOOK_COMPLETION_GUARD` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | lets the flush hook record the accessing worker's queued labels even from inside a completion action |
| `APS5_NO_HOOK_SNAPSHOT` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | takes the GpuMutex on every access instead of using the pending-write snapshot |
| `APS5_NO_HOST_IMPORT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | disables importing guest memory, so address-based shaders copy instead of using it in place |
| `APS5_NO_HW_F32_FLUSH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | skips the startup f32 denormal flush probe and keeps the emulated flush in every shader |
| `APS5_NO_HW_F32_PRESERVE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | stops requesting DenormPreserve for 32-bit floats, so the device keeps its default f32 denormal handling |
| `APS5_NO_IMAGE_PAGE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | leaves the main guest image's page-protection table uncached |
| `APS5_NO_INDIRECT_DRAW` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Pm4.cpp` |  | rejects indirect draw packets at submission validation |
| `APS5_NO_JOIN_WAW_CHECK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | lets a contiguous store join the pending one without the write-after-write check |
| `APS5_NO_KEEP_NEW_STORAGE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | does not keep a newly created storage image alive with the open batch; such an image evicted before the batch completes can hand its memory to another image through the buffer pool while the batch still writes it |
| `APS5_NO_KEYS_FILL_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/DccMetadata.cpp` |  | records no memo of GPU-side DCC key fills |
| `APS5_NO_KEYS_REFILL` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | keeps a covered image's uploaded keys as they were instead of marking them uncompressed after a matching key fill |
| `APS5_NO_KEYS_RESCAN` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | reuses the DCC keys scanned before the lookup's refresh instead of discarding them for a rescan |
| `APS5_NO_KEY_FAST_PATH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/DccMetadata.cpp` |  | scans a surface's DCC keys on every call instead of using and storing a proof |
| `APS5_NO_LABEL_BATCHING` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | turns off every label-batching behaviour, the label runs, queued labels and batched submit |
| `APS5_NO_LABEL_BATCH_SUBMIT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | disables batched label submission |
| `APS5_NO_LABEL_PACKET_FLUSH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketFlush.cpp` |  | disables the label flush between packets, which runs only when batched label submission is off (`APS5_NO_LABEL_BATCH_SUBMIT` or `APS5_NO_LABEL_BATCHING`) |
| `APS5_NO_LABEL_READBACK_STORE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | leaves pending linear and small storage images on the GPU at a label instead of storing them before it |
| `APS5_NO_LABEL_RUNS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records every store in a run of its own between a barrier pair |
| `APS5_NO_LABEL_SHORTCUT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | disables the pending-label table shortcut in a memory wait |
| `APS5_NO_LAYER_CLEAR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | tracks every surface as one layer instead of per array layer |
| `APS5_NO_LAYER_REFUSAL_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | re-checks a layer whose surface check failed at its current generation on every fill |
| `APS5_NO_LAYOUT_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | builds descriptor set layouts fresh instead of through the descriptor cache |
| `APS5_NO_LEASE_MIRROR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | restores copies instead of image mirrors for address-based builds |
| `APS5_NO_LIBRARY_PREBUILD` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | disables prebuilding registration-form shader libraries (only 1 disables it) |
| `APS5_NO_LOCK_PROFILE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | turns off GpuMutex wait profiling under APS5_PROFILE_DRAW |
| `APS5_NO_MESH_WINDOW_BUFFER` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | keeps a mesh program's relative-addressing window in function arrays on NVIDIA's proprietary driver instead of moving it to a storage buffer |
| `APS5_NO_MOVED_BUFFER_TEMPLATES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | keys the full descriptor words of read-only guest buffers instead of allowing a hit when they move |
| `APS5_NO_OBSERVATION_GUARD` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | observes every validated range, not only the ranges whose flush hook waited |
| `APS5_NO_OPPORTUNISTIC_REAP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | skips retiring already-finished batches before a submit |
| `APS5_NO_OWN_IMAGE_REFRESH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | lets a Pending failure of the fast proof take the full walk instead of the own-object refresh |
| `APS5_NO_PARALLEL_MIRROR_REFRESH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | disables the mirror-refresh thread pool, so mirror refreshes run on the calling thread |
| `APS5_NO_PATTERN_FILL_HLE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferFill.cpp` |  | disables the dword-pattern fill HLE |
| `APS5_NO_PAUSE_SPIN` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | disables the CPU pause-spin in a blocked memory wait |
| `APS5_NO_PENDING_HTILE_FILLS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp` |  | reads the HTILE words of a depth target bind even when recorded fills are pending over them, instead of working the words out from the fills |
| `APS5_NO_PERF_FRONTEND_PAIR` | presence | `core/shader/recompiler/Recompiler.cpp` |  | disables the recompiler's frontend-pair reuse; the shader disk cache treats it as output-neutral |
| `APS5_NO_PERF_LAYOUT_PAIR` | presence | `core/shader/recompiler/Recompiler.cpp` |  | disables the recompiler's layout-pair reuse; the shader disk cache treats it as output-neutral |
| `APS5_NO_PIPELINED_INDIRECT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` |  | drains the draw pipeline at every CPU-path indirect draw instead of queuing it behind the pipeline |
| `APS5_NO_PIPELINE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Pipeline.cpp` |  | builds a pipeline per draw instead of reusing cached ones |
| `APS5_NO_PIPELINE_JOURNAL` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/PipelineJournal.cpp` |  | keeps the pipeline journal off; 0 leaves it on, and it is also off while the shader disk cache is disabled |
| `APS5_NO_PIPELINE_LTO` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/PipelineLibrary.cpp` |  | disables background link-time optimization of graphics pipeline libraries |
| `APS5_NO_POOL_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | allocates descriptor pools fresh instead of through the descriptor cache |
| `APS5_NO_PRECOLLECT` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | disables the stage-A precollect pass |
| `APS5_NO_PREPARED_FIND` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | looks the resource-cache key up again instead of using the object stage A found |
| `APS5_NO_PRESENT_EPOCH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | keeps the presenter at collect epoch 0 instead of adopting the newest epoch a worker started |
| `APS5_NO_PRESYNC` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | disables the stage-A pre-sync of pending writes |
| `APS5_NO_PROC_TABLE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | resolves the device's Vulkan entry points per call instead of filling the function table once |
| `APS5_NO_QUEUED_DRAW_READ_ORDER` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawPipeline.cpp` |  | stops queued draws and DMA copies from holding back write-backs over the guest memory they read at commit |
| `APS5_NO_QUEUED_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | disables the queued-label table |
| `APS5_NO_RECENT_IMPORTS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | looks every host import up in the import map instead of first checking the last four found while the map is unchanged |
| `APS5_NO_RECORDED_CLEAR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | waits for a storage image's fast clear in a batch of its own instead of recording it |
| `APS5_NO_RECORDER_SYNC_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | gives the draws the recorder waits for their own batch instead of recording them with the wait |
| `APS5_NO_RECORD_COPIED_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | waits for copied-write draws at once instead of listing their write-back for later |
| `APS5_NO_RECT_INDIRECT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/IndirectDraw.cpp` |  | sends an indirect rectangle-list draw down the CPU path instead of the GPU-side path |
| `APS5_NO_RESIDENT_LINEAR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | drops the residency of linear color targets, so a target whose address has a host import, or whose import a draw would make, is copied in and out again |
| `APS5_NO_RESIDENT_PRESENT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | always presents through guest memory instead of the resident image |
| `APS5_NO_RESIDENT_TARGETS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | copies every render target in and out again instead of keeping it resident |
| `APS5_NO_RESOURCE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | disables the shared resource cache, so every dispatch builds its resources fresh |
| `APS5_NO_RUN_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | walks the lease for a host import's run of ranges on every lookup instead of reusing the bounds found for the same lease snapshot and import window block |
| `APS5_NO_SAMPLED_FROM_STORAGE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | keeps every sampled texture a CPU snapshot instead of viewing the surface's storage image |
| `APS5_NO_SAMPLER_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | creates a sampler per binding instead of caching them |
| `APS5_NO_SEPARATE_QUEUED_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | lets a queued label enter the recorded table and replace a recorded entry of its dword |
| `APS5_NO_SHAPE_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | looks up every draw's resident targets and pipeline anew instead of repeating the ones an earlier draw of the same register shape used |
| `APS5_NO_SINGLE_PASS_COLLECT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | walks the written pages once without clearing them and, when any were written, a second time clearing them, instead of one cleared pass |
| `APS5_NO_SITE_SAMPLING` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | disables per-site sampling of write evidence |
| `APS5_NO_SNAPSHOT_CHECK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | skips the live compare that proves a captured snapshot still equals the guest bytes |
| `APS5_NO_SORTED_SNAPSHOT_LOOKUP` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | scans the snapshot's regions in AddSnapshot instead of searching them |
| `APS5_NO_SPACE_STAMP_ONCE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | stamps every direct import of a cached address space on each hit instead of once per space |
| `APS5_NO_SPARSE_HEAP_MIRROR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | disables sparse heap mirrors, mirroring only whole heap ranges |
| `APS5_NO_STAGE_A_IMAGES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | leaves every image element to the stage-B full lookup instead of a stage-A record |
| `APS5_NO_STORAGE_DEDUPE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | looks each identical consecutive storage descriptor up, and refreshes its image, separately |
| `APS5_NO_SYNC_SITES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | leaves only the per-source and per-thread sync counts, with no per-site attribution |
| `APS5_NO_SYNC_THROUGH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | makes every pending-write sync a full Sync instead of the targeted unlocked wait |
| `APS5_NO_TABLE_REPLAY` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | makes a reused bound image table rerun each element's checks instead of replaying its recorded bind outcomes |
| `APS5_NO_TABLE_REUSE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | disables reusing a materialized image table |
| `APS5_NO_TARGET_WRITEBACK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | skips writing render targets back to guest memory after the draw |
| `APS5_NO_TEMPLATE_DATA_REFRESH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | keys the ShaderData and FlattenedSrt words into compute keys instead of refreshing a template's data buffers |
| `APS5_NO_TEXTURE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | builds every texture and storage-image object fresh instead of caching it |
| `APS5_NO_TEXTURE_HASH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | finds texture-cache entries by scanning the list instead of the hash index |
| `APS5_NO_TIMELINE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | leaves timeline semaphores off, so drains wait under the mutex |
| `APS5_NO_UNIT_SHADOW` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/UnitShadow.cpp` |  | makes every unit shadow primitive inert |
| `APS5_NO_UNLOCKED_DEVICE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | loads the device and runs a dispatch under the GPU mutex instead of using an unlocked device |
| `APS5_NO_UNLOCKED_DRAIN` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketMemory.cpp` |  | keeps the GPU mutex held while draining a packet instead of the unlocked drain |
| `APS5_NO_UNLOCKED_VALIDATE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchLookup.cpp` |  | keeps the dispatch-cache mutex held while validating a cache entry |
| `APS5_NO_UNREGISTERED_DROP` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | stores a storage image's pending results even when its memory is no longer registered, instead of dropping them |
| `APS5_NO_VALIDATE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | validates every draw instead of serving the validation memo |
| `APS5_NO_VALIDATE_SKIP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | syncs every pending write instead of skipping validation on written-element evidence |
| `APS5_NO_VOUCHED_VALIDATION` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/VariantValidation.cpp` |  | validates a cached draw or dispatch through its per-run checks instead of its table block's vouch |
| `APS5_NO_WAIT_OVERLAP_SUBMIT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | disables the unlocked wait that submits recorded work while overlapping writes are open |
| `APS5_NO_WORDWISE_CAPTURE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/ShaderMemory.cpp` |  | fetches every captured page whole instead of reading pending pages word by word |
| `APS5_NO_WORKER_AFFINITY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/WorkerAffinity.cpp` |  | leaves the queue workers and presenter unpinned (affinity mask 0) |
| `APS5_NO_WRITER_KEYED_EVIDENCE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | records written-element evidence without keying it by writer |
| `APS5_NO_WRITE_EVIDENCE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | disables the written-element evidence that lets a read skip the flush hook |
| `APS5_NO_WRITE_ONLY_PASSES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | counts a buffer a draw only stores to as read and written, so the next draw storing to it begins a new render pass |
| `APS5_NO_WRITTEN_SHADOW` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | binds written staging elements in place instead of through the written shadow |
| `APS5_NV_DIAGNOSTICS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | creates the device with VK_NV_device_diagnostics_config and VK_NV_device_diagnostic_checkpoints where supported, for Nsight Aftermath crash dumps |
| `APS5_PACKET_EPOCH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitStatistics.cpp` |  | makes queue workers bump the collect epoch before every packet |
| `APS5_PIPELINED_DRAWS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawPipeline.cpp` | 8 | depth of the queue-0 draw pipeline: 0 records draws inline, 1 selects the default depth of 8, and any other value is the depth capped at 256 |
| `APS5_PIPELINE_DISPATCH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | records an eligible dispatch as an item in the queue-0 pipeline instead of running it inline; an empty value or one starting with `0` leaves it off |
| `APS5_PIPELINE_DRAIN_FILLS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferFill.cpp` |  | runs queue-0 buffer fills through the draw pipeline to keep their order; any value stores them in place instead |
| `APS5_PIPELINE_DRAIN_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | answers a queue-0 wait from the draw pipeline's own pending labels while it is busy; any value drains the pipeline at the wait |
| `APS5_PIPELINE_DRAIN_SUBMISSIONS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | lets a submission end without submitting its recorded work, so it joins a later submit with the pipeline's queue; any value drains the pipeline and submits at the end of the submission |
| `APS5_PIPELINE_JOURNAL_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/PipelineJournal.cpp` | 4096 | number of journal entries loaded at most; a longer journal keeps its last entries, the ones recorded last |
| `APS5_PIPELINE_JOURNAL_MAX_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/PipelineJournal.cpp` | 1024 | size cap in MiB on the pipeline journal; the file is rewritten down to three quarters of the cap when it is reached, keeping the newest pipelines and the modules they name and leaving room for later records |
| `APS5_PIPELINE_JOURNAL_THREADS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/PipelineJournal.cpp` |  | threads that replay the journal's pipelines, 0 disables the replay; unset uses half the hardware threads, at least one |
| `APS5_PIPELINE_STATS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | captures and prints driver statistics for each new pipeline, requiring `VK_KHR_pipeline_executable_properties`; `1` enables |
| `APS5_PIPELINE_STORE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Pipeline.cpp` | adaptive | fixes the LRU bound on the entries of the pipeline store, which otherwise starts at 1024 and doubles up to 4096 for a journal replay that does not fit and while evicted pipelines keep coming back; an entry is evicted only once no recorded draw holds it |
| `APS5_PM4_RING` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/SubmissionRing.cpp` | 16 | number of submissions the PM4 ring keeps for dumps, from 0 to 4096; 0 disables the ring |
| `APS5_POLL_REAP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | reaps completions on every poll tick |
| `APS5_POLL_TRY_EACH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | tries the poll service on every tick instead of at the poll interval |
| `APS5_PREBUILD_THREADS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | threads in the pool that prebuilds registration-form shader libraries and compute pipelines; unset uses half the hardware threads, at least two. Compute pipeline prebuilds, which wait for `APS5_BACKGROUND_COMPILE_KIB`, run on at most half of them (at least one) |
| `APS5_PRESENT_POLL_FENCE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | polls the present fence every 50 us instead of blocking on it |
| `APS5_PROBE_DISPATCH` | string | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | probes one dispatch at a hex program address (optionally `:index`) with debug output |
| `APS5_PROFILE_DRAW` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | prints the per-phase driver and draw timing totals |
| `APS5_PROFILE_GPU` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records the GPU time of recorded work by key from batch timestamps |
| `APS5_PROFILE_GPU_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp`, `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | with APS5_PROFILE_GPU, records the GPU time of each recorded draw by its fragment program's variant id and prints a [drawvariant] line with the code address behind each variant id once (draws that do not use a recipe only; the [gputime] line labels each `draw 0x<variant>`, the id [drawvariant] prints, and the ranges share the batch's query cap) |
| `APS5_REAP_EACH_PACKET` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | reaps deferred labels after every packet |
| `APS5_REGISTRATION_PREPARE_COMMIT_GB` | number | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` | half of physical RAM | stops preparing newly registered shaders ahead of use when process-private memory reaches this GiB budget; `0` disables registration-time preparation |
| `APS5_RELEASE_ON_UNLOCK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | destroys deferred batches on the unlocking thread instead of a release thread |
| `APS5_RELEASE_QUEUE_MAX` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` | 256 | maximum deferred batches handed to the release thread before a thread destroys its own |
| `APS5_RELEASE_UNDER_LOCK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | destroys deferred batches in finish() under the lock instead of on a release thread |
| `APS5_RESOURCE_CACHE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | adaptive | fixes the bound on the entries of the per-device shader-resource cache, which otherwise grows while evicted keys keep coming back and shrinks back to its base when few keys are in use; 0 keeps the adaptive bound |
| `APS5_RESOURCE_CACHE_TABLE_ELEMENTS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 65536 | number of image-table elements the shader-resource cache keeps (0 keeps the default) |
| `APS5_SAMPLE_COMMITTER` | string | `core/libs/prx/libSceAgcDriver/Execution/src/WorkerSampler.cpp` |  | samples queue 0's draw committer every millisecond and writes its stacks to a file (Windows only) |
| `APS5_SAMPLE_THREADS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/WorkerSampler.cpp` |  | samples every thread of the process every 2 ms and writes each busy thread's hottest frames to a file (Windows only) |
| `APS5_SAMPLE_WORKER` | string | `core/libs/prx/libSceAgcDriver/Execution/src/WorkerSampler.cpp` |  | samples the calling queue worker every millisecond and writes its stacks to a file (Windows only) |
| `APS5_SAMPLE_WORKER_QUEUE` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/WorkerSampler.cpp` | 0 | selects the queue worker sampled by `APS5_SAMPLE_WORKER`, accepting decimal or hexadecimal IDs such as `0x50`; process and committer sampling remain independent |
| `APS5_SHADOW_VERIFY` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/UnitShadow.cpp` |  | verifies that the shadow-published bytes equal the guest bytes |
| `APS5_SHARED_COLLECT_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | uses the tracker's shared collect-memo ring under its mutex instead of the per-thread ring |
| `APS5_SHARED_MEMORY_COUNTERS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | accumulates the memory counters into one shared set with atomic RMWs |
| `APS5_SKIP_INVALID_INDIRECT_REGISTERS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | skips the entries whose offset dword is no register offset in a SET_*_REG_INDIRECT list read again where its packet executes, and writes its valid entries, instead of stopping |
| `APS5_SKIP_REFUSED_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Diagnostics.cpp` |  | skips a draw whose graphics stage the recompiler refuses, reporting each refused shader once, instead of failing the queue |
| `APS5_SKIP_REQUEST_DUMPS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderDiagnostics.cpp` | 8 | caps RecompileRequest dumps per shader-skip reason |
| `APS5_SKIP_SHADER` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Diagnostics.cpp` | 0 | hex program address of a shader whose draws and dispatches are dropped |
| `APS5_SKIP_SHADER_FILE` | path | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Diagnostics.cpp` |  | file of whitespace-separated hex program addresses (`#` starts a comment) whose draws and dispatches are dropped; re-read within 500 ms of a change; a token that is not a whole 64-bit hex number is reported and ignored |
| `APS5_SNAPSHOT_CHECK_FATAL` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | throws on the first snapshot that differs from registered memory instead of only counting them |
| `APS5_SPANNED_WAVE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | spans a single-wave wave64 compute program over two full 32-lane subgroups |
| `APS5_SPIRV_OPT` | string | `core/shader/recompiler/SpirvBackend/src/SpirvOptimizer.cpp` |  | selects the SPIR-V optimization pipeline: `full` restores the performance pipeline, `none` skips optimization |
| `APS5_SRGB_ATTACHMENT_PROXY` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | forces the sRGB color-attachment proxy format; `1` forces it |
| `APS5_SRGB_SHADER_DECODE` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp` |  | `1` forces shader sRGB decode for the sRGB 8-bit formats; `copy` does the same, except that 8 and 8_8 sRGB sample an RGBA8 sRGB copy when the device can blit to RGBA8; any other value has no effect |
| `APS5_STAGING_POOL_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/BufferPool.cpp` |  | size in MiB of the device-local staging buffer pool; `0` disables the device tier |
| `APS5_STALE_EVICT` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | also evicts a proved-present dead image's pending layers on an access |
| `APS5_STALL_WATCHDOG_SECS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/SubmissionRing.cpp` | 0 | seconds a queue may go without progress before the submission rings are dumped; 0 disables the watchdog, at most 86400 |
| `APS5_STORE_AT_FLIP` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | stores every pending storage-image result at each display flip; only `1` is accepted |
| `APS5_STORE_AT_LABEL` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | flushes every pending storage-image result when a label is written on the GPU; only `1` is accepted |
| `APS5_SUBMISSION_EPOCH_EACH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | bumps the guest-memory collect epoch on every submission instead of only when the submission is newer than the last bump |
| `APS5_SUBMIT_AT_END` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | submits recorded work at the end of a submission even when a worker is queued |
| `APS5_SUBMIT_PROGRESS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/Submission.cpp` |  | enables opt-in pacing for eligible queue-zero submission prefixes within a shared nominal two-millisecond acknowledgement/timeline budget; timeout retains asynchronous acceptance |
| `APS5_SUSPEND_DRAIN` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | drains the device at a suspend point |
| `APS5_SYNC_COMPLETION_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | keeps every draw with completion work synchronous |
| `APS5_SYNC_DISPATCH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | waits for every dispatch instead of batching it |
| `APS5_SYNC_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | makes every draw synchronous |
| `APS5_SYNC_FLIP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketMemory.cpp` |  | makes flips synchronous (no frames in flight) |
| `APS5_SYNC_LEASE_DISPATCH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | waits for leased draws and dispatches at once instead of deferring them |
| `APS5_SYNC_SAMPLE_DUMPS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | takes the CPU path for occlusion-counter sample dumps, as `APS5_NO_GPU_SAMPLE_DUMPS` does |
| `APS5_SYNC_SHADER_PREPARE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | prepares registered shaders on the registering thread instead of in the background |
| `APS5_SYNC_TEXTURE_UPLOAD` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | restores the waiting batch for every texture upload |
| `APS5_TEMPLATE_AUDIT_MS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 1000 | milliseconds after which a cached shader-resource object takes the full walk instead of the fast proof; `0` turns the audit off |
| `APS5_TEXTURE_CACHE_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 0 | forced byte budget in MiB for the storage and sampled texture caches; `0` selects the budget automatically |
| `APS5_THREAD_NICE` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/ThreadPriority.cpp` | -10 | nice value the raised queue workers and presenter ask rtkit for (clamped to -20..19) |
| `APS5_THREAD_PRIORITY` | string | `core/libs/prx/libSceAgcDriver/Execution/src/ThreadPriority.cpp` | off | raises the queue workers and presenter above the title's threads: `off`, `high` or `rt`; on macOS `high` is the user-interactive QoS class and `rt` a Mach time-constraint policy (period 1 ms, computation 0.5 ms) |
| `APS5_THREAD_RT` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/ThreadPriority.cpp` | 10 | realtime priority the raised threads ask rtkit for (clamped to 1..99) |
| `APS5_THREAD_RTTIME_US` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/ThreadPriority.cpp` | 200000 | hard RLIMIT_RTTIME the realtime plan lowers to, in microseconds |
| `APS5_TRACE_BARRIERS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | prints the first n accesses for which the hazard validator would skip a barrier; needs `APS5_BARRIER_VALIDATE` |
| `APS5_TRACE_BENCH` | string | `core/libs/prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp` |  | `1` with `APS5_TRACE_SUMMARY_EVERY` also rewrites `aps5-bench-<commit>-<pid>.json.gz` with the frame times of the last 240 intervals (older ones are counted as dropped); any other value, or `1` without `APS5_TRACE_SUMMARY_EVERY`, fails the first present |
| `APS5_TRACE_BIGBUF` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | lists every copied guest region of at least 1 MiB |
| `APS5_TRACE_CAPSYNC` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | prints every unlocked hook wait |
| `APS5_TRACE_CONTEXT_STATE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Pm4.cpp` |  | logs context-state operations and the context registers written |
| `APS5_TRACE_COPY_HLE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` |  | traces the CPU copy HLE's refusals |
| `APS5_TRACE_DCC_KEYS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/DisplayBuffer.cpp` |  | traces stores of uncompressed DCC keys |
| `APS5_TRACE_DIR` | string | `core/libs/prx/libSceAgcDriver/Execution/src/PerformanceTrace.cpp` | . | directory for the trace and summary files |
| `APS5_TRACE_DISPATCH_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | reports dispatch-cache entries whose key changed |
| `APS5_TRACE_DISPATCH_IO` | string | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | prints every dispatch's resources; `2` also lists the small-buffer words |
| `APS5_TRACE_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | prints one line per draw with its phases and inputs |
| `APS5_TRACE_DRAW_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCacheStatistics.cpp` |  | reports draw-cache entries whose key changed |
| `APS5_TRACE_DRAW_PROGRAMS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` |  | prints one line per recorded draw with the stage, role and code address of each program |
| `APS5_TRACE_EVENTS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/PerformanceTrace.cpp` |  | number of events a windowed trace keeps in its buffer, 1 through 4,194,304; the default is 262,144 |
| `APS5_TRACE_FILL_COVER` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | prints the first fills that meet images without covering them |
| `APS5_TRACE_FLUSH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | names what forces pending storage-image results into guest memory |
| `APS5_TRACE_FRAMES` | string | `core/libs/prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp` |  | records `<count>` presents from present `<start>` (whole numbers, `start:count`) |
| `APS5_TRACE_GPU` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/DisplayBuffer.cpp` |  | prints GPU wait, submission and scanout events, the execute lines, and waits longer than 200 ms (read in DisplayBuffer, PacketExecution, QueueWorker, Submission and WaitMemory) |
| `APS5_TRACE_HOST_IMPORT` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | traces host-import mapping failures |
| `APS5_TRACE_INDIRECT_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawPrecheck.cpp` |  | prints indirect draw packets that reach the precheck |
| `APS5_TRACE_LATE_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | prints how late labels were trusted or refused |
| `APS5_TRACE_MEMORY_BUDGET` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/MemoryAccounting.cpp` |  | tracks Vulkan allocation bytes by heap and category, separates active and retained memory, and reports dynamic storage-cache budget terms at most every 10 seconds |
| `APS5_TRACE_PREPARATION_WAIT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | prints where a demanded shader preparation sat in the queue |
| `APS5_TRACE_QUERY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | on Windows, prints every 16th page query with its cost |
| `APS5_TRACE_RESOURCE_KEY_MISSES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | reports shader-resource cache key misses |
| `APS5_TRACE_SECONDS` | string | `core/libs/prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp` |  | records from the first present `<start>` seconds after the first present for `<length>` seconds (whole numbers, `start:length`) |
| `APS5_TRACE_SHADER_CORRELATION` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` |  | traces shader selection and preparation failure tokens through draw batches (requires `APS5_CAPTURE_TRACE`) |
| `APS5_TRACE_SHADER_REGS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | prints a shader's registers for one hex program address, or `all` |
| `APS5_TRACE_SHADOW` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/UnitShadow.cpp` |  | prints the first n shadow publishes, the first 64 seeds, every evict and retire |
| `APS5_TRACE_SKIPS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderDiagnostics.cpp` |  | reports each distinct shader skip once with or without it; set, a repeat of an already reported skip is also logged as `again`, cut to 100 characters |
| `APS5_TRACE_STAGING` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | prints every distinct staged range once, with its size and whether an atomic element lies in it |
| `APS5_TRACE_SUMMARY_EVERY` | integer | `core/libs/prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp` |  | seconds between periodic trace summaries, emitted on the next present |
| `APS5_TRACE_TEXTURES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | prints a sampled texture's bytes and how many are nonzero |
| `APS5_TRACE_UNREADABLE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | names the code that checked an inaccessible range |
| `APS5_TRACE_UPLOAD` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | names why a storage image is uploaded again |
| `APS5_UNIT_SHADOW_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/UnitShadow.cpp` | 1024 | budget in MiB of the unit shadow's slabs |
| `APS5_UNIT_SHADOW_SLAB_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/UnitShadow.cpp` | 8 | size in MiB of one unit-shadow slab |
| `APS5_VALIDATE_LEGACY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | uses the legacy snapshot and full-compare path for pending-write validation |
| `APS5_VALIDATE_SYNC_SKIP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | reads skipped dwords through the hook too and counts a mismatch |
| `APS5_VERIFY_BDA_TABLES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | checks a reused BDA page table against the one just built |
| `APS5_VERIFY_DATA_HITS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` |  | recomputes a dispatch data hit and aborts on a disagreement |
| `APS5_VERIFY_DRAW_DATA_HITS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` |  | recomputes a draw data hit and aborts on a disagreement |
| `APS5_VERIFY_DRAW_ENTRIES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` |  | re-decodes every draw-cache hit through the full path and checks it |
| `APS5_VERIFY_DRAW_PIPELINE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | with `APS5_DRAW_PIPELINE` set, runs the full check instead of every 64th reused epoch proof on each thread and prints `[epoch-proofs]` counts |
| `APS5_VERIFY_DRAW_RECIPE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` |  | re-decodes a draw recipe instead of using the cache and checks the result |
| `APS5_VERIFY_DRAW_SNAPSHOTS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | re-reads a reused draw snapshot and counts the ones that differ |
| `APS5_VERIFY_KNOWN_VALUES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` |  | serves known values through the hook too and counts mismatches |
| `APS5_VERIFY_PROOFS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | runs the full walk beside every own-object refresh proof and aborts on a different decision |
| `APS5_VERIFY_RECIPE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | makes the ordinary prepare path beside a recipe hit and compares the answers |
| `APS5_VERIFY_REGISTER_KEYS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawRegisterKey.cpp` |  | checks the incremental register fingerprint against the full state |
| `APS5_VERIFY_SET_LAYOUTS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | checks the draw's set layouts against those described from the shaders |
| `APS5_VERIFY_SHAPE_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | looks up the resident targets and pipeline of every shape memo hit anew as well, throws when they differ and prints `[shape-memo]` counts of the checked hits every 10 s |
| `APS5_WAIT_LOCK` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | always takes the GPU mutex on a wait (no unlocked label lookup) |
| `APS5_WAIT_LOCK_ENTRY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | takes the GPU mutex blocking on the entry instead of trying it |
| `APS5_WATCH_MEMORY` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | watches named guest ranges (`<hex address>:<hex bytes>[,...]`) across dispatches; needs `APS5_SYNC_DISPATCH=1` |
| `APS5_WORKER_AFFINITY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/WorkerAffinity.cpp` |  | pins the queue workers and presenter to the performant cores of a hybrid CPU (needed to enable it on non-Linux hosts) |
| `APS5_WORKER_AFFINITY_MASK` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/WorkerAffinity.cpp` |  | explicit hex CPU affinity mask for the queue workers and presenter |
| `APS5_WORKING_SET_MIB` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | sets the process working-set minimum to this many MiB (Windows only) |
| `APS5_WRITE_EVIDENCE_AFTER` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` | 4 | consecutive unchanged observations before a dword's written-element evidence is trusted |
| `APS5_WRITE_EVIDENCE_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` | 16 | largest written range in KiB that the evidence may serve |
| `APS5_WRITE_EVIDENCE_SAMPLE` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` | 16 | reads one in this many pending writes to sample them |
| `APS5_WRITE_WATCH_IMPORTS` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | probe | how host imports are write-tracked, not on Windows: `probe`, `watch` or `unwatch`; any other value throws |
| `APS5_WRITTEN_SHADOW_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 2048 (8192 on a discrete GPU) | upper bound in KiB of the written staging window; a set value is used as given on both device types |
| `APS5_WRITTEN_SHADOW_MIN_KIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 16 | lower bound in KiB of the written staging window |

### libSceAjm.native

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_TRACE_AJM` | presence | `core/libs/prx/libSceAjm.native/src/Ajm.cpp` |  | logs every job the title submits and what the library writes back |

### libSceAudioOut

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_PAD_SPEAKER_FALLBACK_DB` | string | `core/libs/prx/libSceAudioOut/src/AudioOut2PadMix.cpp` | 0 | gain in dB for AudioOut2 controller-speaker ports folded into the main mix when no controller speaker is present; accepts a number from -60 to 0 or mute/off/-inf, and the default 0 leaves the mix unchanged; any other value, including an out-of-range number, stops the title with an error naming the variable and the value |
| `ANYPS5_MUTE_ON_FOCUS_LOSS` | string | `core/libs/prx/libScePad/src/HostVolumeState.cpp` | 0 | `1` mutes the host audio while the display window has no focus and restores it when the focus returns; `0` or unset leaves the audio alone; any other value, including an empty one, stops the game with an error naming the variable |
| `ANYPS5_VOLUME` | string | `core/libs/prx/libScePad/src/HostVolumeState.cpp` | 100 | initial host output volume, a whole number from 0 to 100, changed afterwards by the volume keys; 100 leaves the audio bit-identical to the title's output; any other value, including an empty one, stops the game with an error naming the variable |
| `APS5_TRACE_AUDIOOUT` | presence | `core/libs/prx/libSceAudioOut/src/AudioOut.cpp` |  | logs each audio-out port opened with its type, sample count, frequency and format |
| `APS5_TRACE_AUDIOOUT2` | presence | `core/libs/prx/libSceAudioOut/src/AudioOut2Context.cpp` |  | prints the title's AudioOut2 context and port parameters and stream activity |

### libSceFiber

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_TRACE_FIBER` | presence | `core/libs/prx/libSceFiber/Export.cpp` |  | logs fiber initialization, switches and returns to the thread |

### libSceFont

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_SYSTEM_FONTS` | string | `core/libs/prx/libSceFont/src/SystemFont.cpp` |  | directory holding the system fonts, defaulting to anyps5-fonts beside the executable |

### libSceIme

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_PSN_DIALOGS` | string | `core/libs/prx/libSceIme/src/PsnDialogNotice.cpp` | silent | when a dialog that needs PlayStation Network opens (profile, friend list, store, invitations, Facebook link, authorized apps), `show` displays a host notice that the feature is not available; `silent`, unset or empty shows nothing, as before; any other value stops the title at its first such dialog with an error naming the variable |

### libSceNpEntitlementAccess

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_ENTITLEMENTS` | string | `core/libs/prx/libSceNpEntitlementAccess/Export.cpp` |  | path of the entitlements ini listing owned add-ons, defaulting to anyps5-entitlements.ini beside the executable |

### libSceNpTrophy2

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_TRACE_UDS` | presence | `core/libs/prx/libSceNpTrophy2/src/TitleTrophies.cpp` |  | prints the UDS events contexts post and the stat changes trophy rules apply, and the trophy files a lookup does not find (read in NpUniversalDataSystem/Event and NpTrophy2/TitleTrophies) |

### libScePad

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_RUMBLE` | integer | `core/libs/prx/libScePad/include/PadRumbleIntensity.hpp` | 100 | host controller vibration strength as a whole percentage from 0 to 100, read once when the pad input starts and again by the audio-out controller sound card path; 100 or unset leaves vibration exactly as the title asks, 0 turns motor rumble, impulse-trigger rumble and controller-sound-card haptics off, in between scales the motor amplitudes with rounding and the haptic samples linearly; DualSense adaptive trigger effects are not scaled; any other value (empty, signed, fractional, above 100) stops the game with an error naming the variable |
| `ANYPS5_PAD_IDENTIFY` | string | `core/libs/prx/libScePad/include/PadIdentify.hpp` | off | which controller-identification pulse to send when a controller is bound to a player slot: `off` sends none, `connect` sends the pattern once per binding (player N gets N short pulses), `always` also repeats it every 30 seconds while the title is not rumbling; the pulse is scaled by `ANYPS5_RUMBLE`, never overrides a title's rumble, and any other value (empty, uppercase, a list) stops the game with an error naming the variable |
| `APS5_PAD_INPUT_FILE` | string | `core/libs/prx/libScePad/src/PadState.cpp` |  | path of the file polled for injected pad button and stick commands |

### libScePlayGo

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_PLAYGO_UNLISTED_CHUNKS` | string | `core/libs/prx/libScePlayGo/Export.cpp` |  | answer for a chunk ID the playgo manifest does not list: installed or absent; unset throws instead of answering |

### libSceSaveData.native

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_SAVEDATA_TRACE` | presence | `core/libs/prx/libSceSaveData.native/Export.cpp` | 3 | logs every save-data operation instead of the first three at each logging site |
| `ANYPS5_FREE_SPACE` | string | `core/libs/prx/libSceSaveData.native/Export.cpp` | fixed | `fixed` keeps the save sizes and room the title sees today; `host` refuses to create a save whose blocks the save's volume cannot hold (`SCE_SAVE_DATA_ERROR_NO_SPACE_FS`, with the missing blocks in `required_blocks`) and bounds a save's free blocks by the room its volume has; empty or any other value stops the game with an error naming the variable; on Windows an empty value is the same as unset, because `cmd` and PowerShell remove the variable |

### libSceSystemService

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_LANGUAGE` | integer | `core/libs/prx/libSceSystemService/SystemService.hpp` | 1 | console language number from 0 to 30 returned by sceSystemServiceParamGetInt, defaulting to English US; any other value throws |

### libSceUsbStorage

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_USB_STORAGE_PASSTHROUGH` | presence | `core/libs/prx/libSceUsbStorage/src/HostVolumes.cpp` |  | passes the FAT and exFAT volumes the host mounted from USB mass-storage devices through to the guest, read-only where the host mounted them read-only; unset, no host volume is scanned and the guest sees no USB storage device |

### libSceVideoOut

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_DISPLAY_INDEX` | integer | `core/libs/prx/libSceVideoOut/src/DisplayWindow.cpp` | 0 | SDL display index used for the initial window size and centered position; invalid or unavailable indices fail window creation; selection does not guarantee the window will not activate |
| `ANYPS5_FULLSCREEN` | presence | `core/libs/prx/libSceVideoOut/src/DisplayWindow.cpp` |  | opens the display window fullscreen at the desktop resolution when set to a non-empty value other than 0; F11 still switches back |
| `ANYPS5_INPUT_CONFIG` | string | `core/libs/prx/libSceVideoOut/src/InputMapping.cpp` |  | path of the input mapping file that binds host keys and mouse to guest pad actions |
| `ANYPS5_PAD_PORT0_STATE` | string | `core/libs/prx/libSceVideoOut/include/Port0Presence.hpp` | default | `default` keeps port 0 connected and user 0 logged in as before; `controller` makes port 0 and user 0 follow the controller bound to slot 0, a change committed after 500 ms, and a withdrawn port 0 reads zeroed and disconnected; any other value, including an empty one, stops the game when video output starts with an error naming the variable |
| `ANYPS5_REQUIRE_DISPLAY` | presence | `core/libs/prx/libSceVideoOut/tests/Flip.cpp` |  | makes video-out tests fail instead of skipping when no display or Vulkan device is available |
| `APS5_NO_PAD_INPUT` | presence | `core/libs/prx/libSceVideoOut/src/PadInput.cpp` |  | ignores keyboard and mouse input so a measurement run is not advanced by a stray press |
| `APS5_NO_WGI` | presence | `core/libs/prx/libSceVideoOut/src/VideoOutDriver.cpp` |  | turns off SDL's Windows.Gaming.Input joystick backend at default hint priority, on Windows hosts where it faults a few seconds into every run |
| `APS5_OUTPUT_RESOLUTION` | string | `core/libs/prx/libSceVideoOut/src/VideoOutDriver.cpp` |  | `<width>x<height>` in pixels that replaces the host display's size when `sceVideoOutGetOutputStatus` and `sceVideoOutGetOutputStatusWithVrr` report the output resolution: 1 for a size below 3840 wide and 2160 high, 2 otherwise, as for the display; unset or empty follows the display, and any other value makes those calls throw |
| `APS5_VIDEOOUT_TELEMETRY_DIR` | string | `core/libs/prx/libSceVideoOut/src/Telemetry.cpp` |  | directory where videoout.jsonl is appended with the pad and present telemetry lines; nothing is opened when unset or empty |

### libSceVideodec2

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_TRACE_VIDEODEC` | presence | `core/libs/prx/libSceVideodec2/Export.cpp` |  | logs decoder memory queries, queue allocation and the first few decodes |

### libc

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_LOG_FILE` | string | `core/libs/prx/libc/src/LogFilePlatform.cpp` |  | path of a file that also receives everything the converted executable writes to standard output and standard error, appended line by line, while the terminal output goes on; relative paths resolve against the working directory; an empty value is the same as unset; a file that cannot be opened stops the executable at load with status 1 and an error naming the variable, the path and the operating system error |
| `APS5_CRASH_INDIRECT_MEMORY` | presence | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | adds to the Windows crash report up to 64 bytes from each pointer in the dump of the words the registers point at, as `[[reg+off]] @addr` lines; off when unset or empty |
| `APS5_CRASH_STACK_BYTES` | integer | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` | 0 | bytes of the faulting thread's stack from rsp, decimal or 0x hexadecimal and at most 1048576, that the Windows crash report prints with the data at the stack words pointing outside the stack; 0 prints none |
| `APS5_HANG_DUMP_SECS` | integer | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | on Windows, whole seconds after which a watchdog dumps all threads and aborts; 0 or a value that is not a decimal number is ignored |
| `APS5_HEAP_CACHE_MIB` | integer | `core/libs/prx/libc/src/GuestHeap.cpp` | 4096 | size in MiB of the cache of freed large blocks before the oldest are decommitted |
| `APS5_HELPER_AFFINITY_MASK` | integer | `core/libs/prx/libc/include/CpuTopology.hpp` |  | hex core mask that helper threads (deferred release, mirror refresh) are pinned to |
| `APS5_LOG_TIMESTAMPS` | presence | `core/libs/prx/libc/include/general/LogMacros.hpp` |  | prefixes each log line with `[+S.UUUUUU s]`, the seconds with microsecond digits since the clock started at the first stamped line; on Windows each library has its own clock, started at that library's first stamped line |
| `APS5_NO_PROCMAP_QUERY` | presence | `core/libs/prx/libc/include/specifics/linux/HostMapping.hpp` |  | skips the /proc/self/maps PROCMAP_QUERY ioctl and scans the maps file instead |
| `APS5_NO_SSE4A_EMULATION` | presence | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | disables emulation of AMD-only SSE4a instructions, making such a fault fatal |
| `APS5_NO_WRITE_WATCH` | presence | `core/libs/prx/libc/src/GuestArena.cpp` |  | disables write watching so guest buffers are fully re-read on every use |
| `APS5_PIN_WAIT_MS` | integer | `core/libs/prx/libc/src/GuestAllocations.cpp` | 60000 | milliseconds to wait for a pinned allocation lease before failing |
| `APS5_TRACE_AFFINITY` | presence | `core/libs/prx/libc/include/CpuTopology.hpp` |  | prints a line naming the role, tid and mask of every thread pinning |
| `APS5_TRACE_CALLS` | string | `core/libs/prx/libc/src/specifics/x86_64/CallTrace.cpp` |  | comma-separated libraries (or all) whose guest calls are traced |
| `APS5_TRACE_CALLS_FILE` | string | `core/libs/prx/libc/src/specifics/x86_64/CallTrace.cpp` |  | file rewritten every 10 seconds with each traced import called so far and its number of calls |
| `APS5_TRACE_EXIT` | presence | `core/libs/prx/libc/src/Process.cpp` |  | prints a backtrace of the caller of exit, _Exit and the process-exit path |
| `APS5_TRACE_SSE4A` | presence | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | prints each emulated SSE4a instruction once per rip |
| `APS5_VERIFY_DIRECT_COLLECT` | presence | `core/libs/prx/libc/include/WindowsMappings.hpp` |  | queries the regions of each private-memory collect read from the allocation map and throws when that collect succeeded over memory that is no longer committed private |
| `APS5_WATCH_ADDR` | integer | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | absolute hex address whose writes are reported after its page is committed |
| `APS5_WATCH_WRITE` | integer | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | guest ELF vaddr whose 8-byte writes are reported by single-stepping the protected page |

### libkernel

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_LOADEXEC_ARGV_SKIP` | string | `core/libs/prx/libkernel/System/src/Process.cpp` |  | on Windows, set to `1` to drop the first process argument and clear the variable |
| `APS5_COARSE_TIMED_WAITS` | presence | `core/libs/prx/libkernel/Time/TimedWait.cpp` |  | on Windows, rounds timed waits up to the scheduler tick |
| `APS5_GAME_MODE` | string | `core/libs/prx/libkernel/System/src/HostPerformanceProcess.cpp` | off | asks the host for its game or performance mode while the title runs, read when libkernel loads: `off`, `on` or `auto`; on Linux it requests Feral GameMode (`libgamemode.so.0`, loaded with dlopen) and on macOS it holds a user-initiated, latency-critical `NSProcessInfo` activity; `on` logs a missing library or backend once, `auto` stays silent about it, and a refused request is logged once in both; nothing here is fatal, and any other value stops the title with an error naming the variable |
| `APS5_JOB_AFFINITY` | presence | `core/libs/prx/libkernel/Pthread/src/Thread.cpp` |  | on Windows, pins the title's job worker threads to the efficiency cores of a hybrid CPU |
| `APS5_JOB_AFFINITY_MASK` | integer | `core/libs/prx/libkernel/Pthread/src/Thread.cpp` |  | on Windows, hex core mask that job worker threads are pinned to |
| `APS5_MIMALLOC` | presence | `core/libs/prx/libkernel/System/src/MimallocMacOs.cpp` |  | makes the process allocate through mimalloc instead of the system allocator: on Windows the entry stub (`core/relinker/elfpatcher/src/windows/WindowsEntryStubBuilder.cpp`) loads `mimalloc.dll`, on macOS libkernel restarts the title with `libmimalloc.dylib` in `DYLD_INSERT_LIBRARIES` |
| `APS5_MIMALLOC_RESTARTED` | presence | `core/libs/prx/libkernel/System/src/MimallocMacOs.cpp` |  | on macOS, set by libkernel for the restart that loads mimalloc and removed at start-up, so a restart that did not load it is not repeated |
| `APS5_NO_DIRECT_MEMORY_SECTION_POOL` | presence | `core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp` |  | on Windows, creates a section for every direct-memory allocation and closes it at release instead of reusing released sections that never held guest data |
| `APS5_NO_JOB_AFFINITY` | presence | `core/libs/prx/libkernel/Pthread/src/Thread.cpp` |  | on Windows, disables pinning of job worker threads |
| `APS5_NO_SLEEP_WAKE` | presence | `core/libs/prx/libkernel/Time/SleepWake.cpp` |  | keeps every sleep of 1 ms or less in full instead of ending it at a GPU batch, batch completion action or flip completion |
| `APS5_NO_TIMER_RESOLUTION` | presence | `core/libs/prx/libkernel/Time/Time.cpp` |  | keeps the default 15.6 ms scheduler tick instead of raising it to 0.5 ms on Windows |
| `APS5_TIME_SCALE` | string | `core/libs/prx/libkernel/Time/Time.cpp` | 1 | factor applied to the guest monotonic clocks and TSC; sleeps and host JobManager worker spin intervals still take real time; fractional factors work, and a value that is not above 0 uses 1 |
| `APS5_TRACE_APR` | presence | `core/libs/prx/libkernel/Apr/src/Apr.cpp` |  | logs every APR file read with its path, offset, size and destination |
| `APS5_TRACE_MEMORY` | presence | `core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp` |  | logs direct-memory map, remap, protect, unmap and physical allocation operations |
| `APS5_TRACE_SYNC` | presence | `core/libs/prx/libkernel/EventFlag/src/EventFlag.cpp` |  | logs event-flag and semaphore operations |
| `APS5_TRACE_TIMERS` | presence | `core/libs/prx/libkernel/Time/Time.cpp` |  | on Windows, runs a timed-wait selftest at startup and prints mean, p50 and max latencies |
| `APS5_TRACE_USLEEP` | presence | `core/libs/prx/libkernel/Time/Time.cpp` |  | reports every 2000 usleep calls the guest call sites that sleep most |
| `APS5_TRACE_WAITS` | presence | `core/libs/prx/libkernel/Time/Time.cpp` |  | aggregates blocking waits per guest call site and prints the longest-waiting sites every 3 s |

