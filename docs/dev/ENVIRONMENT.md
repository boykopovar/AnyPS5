# Environment variables

The code reads environment names through `getenv`, `IsEnvironmentSet`, `kibSetting`, `MaskFromEnvironment`, `EnvInt`, `GetEnvironmentVariableA`, `GetEnvironmentVariableW` with a wide literal, `configured`, `PipelineSwitch`, `environmentCount`, `EnvironmentNumber` and `addWideString` (the wide names the Windows entry stub reads). This table lists every `APS5_*` and `ANYPS5_*` name it reads, its kind, the value used when it is unset and what it does, one table per library. Regenerate the names with `python3 tools/env_reference.py --table`; the Conventions check fails when a name the code reads has no row here, or a row's name is no longer read.

### core/libs

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_JOB_MANAGER_CASE` | string | `core/libs/tests/JobManagerReference.cpp` |  | selects the CPU fixture run by `tools/run_job_manager_reference.py` |
| `ANYPS5_JOB_MANAGER_HOST` | presence | `core/libs/tests/JobManagerReference.cpp` |  | runs the reference adapter against the host fallback instead of the loaded guest module |
| `ANYPS5_LOADEXEC_PARENT` | string | `core/libs/tests/GuestLoadExecHost.cpp` |  | variable the Windows entry stub reads; its wide name is built in `core/relinker/elfpatcher/src/windows/WindowsEntryStubBuilder.cpp` |
| `ANYPS5_LOADEXEC_TEST_ROOT` | string | `core/libs/tests/GuestLoadExecHost.cpp` |  | directory the load-exec host test reads its guest executable from |
| `ANYPS5_TEST_QUOTED_ARGV0` | presence | `core/libs/tests/GuestLoadExecHost.cpp` |  | makes the load-exec test's first argument one with quotes, a backslash and non-ASCII text |
| `APS5_EBOOT` | string | `core/libs/tests/GuestSystemExit.cpp` |  | host path of the executable launched for /app0/eboot.bin, defaulting to the running executable |
| `APS5_SKIP_TIMING_TESTS` | presence | `core/libs/tests/PreciseWait.cpp` |  | treats the timing tests' runner as preempted, so they skip without probing the clock |
| `APS5_USBD_DEVICE` | string | `core/libs/tests/GuestUsbd.cpp` |  | selects the USB device a test drives, as vendor:product in hex |

### core/relinker

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_GUEST_MODULE_ARGS` | string | `core/relinker/elfpatcher/tests/GuestModuleArgsHost.cpp` |  | path of the file the module-argument fixture appends its events to |

### core/shader

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_NO_SHADER_CACHE` | presence | `core/shader/recompiler/ShaderDiskCache.cpp` |  | disables the on-disk shader cache when set to a non-empty value other than 0 |
| `ANYPS5_SHADER_CACHE_DIR` | string | `core/shader/recompiler/ShaderDiskCache.cpp` |  | directory for the on-disk shader cache, overriding the one beside the executable |
| `APS5_BDA_BYTE_READS` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvBdaRead.cpp` |  | forces one address lookup and byte load per byte for BDA reads instead of a wide dword load |
| `APS5_BDA_DWORD_STORES` | presence | `core/shader/recompiler/Translation/src/MemoryInstructions/FlatInstructions.cpp` |  | reverts BDA writes to one address store per dword instead of a wide multi-dword store |
| `APS5_DUMP_IR` | string | `core/shader/recompiler/Recompiler.cpp` |  | hex code address (or all) of the programs whose IR is printed after each front-end pass |
| `APS5_EXACT_RCP` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvAlu/SpirvAluEmitterReciprocal.cpp` |  | emits `v_rcp_f32` and `v_rcp_iflag_f32` as the RDNA hardware reciprocal (bit-exact for normal inputs and results) instead of the driver's division; adds about 13k SPIR-V words to each shader that uses them |
| `APS5_GATHER_CONST` | integer | `core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp` |  | integer every non-depth gather returns in all four texels instead of the sampled value |
| `APS5_IMAGE_TABLE_KEY_SCAN` | integer | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` | 256 | number of image-table entries the key scan examines |
| `APS5_IMAGE_TABLE_STRICT` | presence | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` |  | sets image-table materialization to its strict form; 0 leaves it off |
| `APS5_NO_ATOMIC_BARRIER` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvMemory/SpirvFormatConvert.cpp` |  | drops the device-scope acquire/release memory barrier emitted after atomics |
| `APS5_NO_ATOMIC_ZERO_SKIP` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvMemory/SpirvMemoryInstructions.cpp` |  | restores plain buffer atomics instead of skipping identity-operand atomics |
| `APS5_NO_CODE_HASH_KEY` | presence | `core/shader/recompiler/CacheKey.hpp` |  | appends every code word to the cache key instead of hashing the code |
| `APS5_NO_FAILURE_MEMO` | presence | `core/shader/recompiler/Recompiler.cpp` |  | rebuilds a program whose compilation failed instead of reusing the memoized failure |
| `APS5_NO_IMAGE_TABLES` | presence | `core/shader/recompiler/Optimization/src/ResourceTracker.cpp` |  | disables image tables, leaving table-loaded descriptors unplanned |
| `APS5_NO_PERF_DISK_PLANS` | presence | `core/shader/recompiler/Recompiler.cpp` |  | disables the disk cache of planning results |
| `APS5_NO_PERF_DOM_INTERSECTION` | presence | `core/shader/recompiler/ControlFlow/src/Structurizer.cpp` |  | computes dominator intersections into a fresh vector instead of intersecting in place |
| `APS5_NO_PERF_ITERATION_SELECTION` | presence | `core/shader/recompiler/ControlFlow/src/Structurizer.cpp` |  | recomputes the post-dominator sets when finding a loop's iteration-selection merge instead of reusing them |
| `APS5_NO_RESULT_MEMO` | presence | `core/shader/recompiler/Recompiler.cpp` |  | materializes a fresh result for every recompile instead of reusing the result memo |
| `APS5_NO_SRT_TAPE` | presence | `core/shader/recompiler/Optimization/src/SrtWalker/SrtTape.cpp` |  | runs the runtime-source walk of a plan in the IR evaluator instead of the compiled tape |
| `APS5_NO_TABLE_RESOLUTION_REUSE` | presence | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` |  | stops a result memo miss from reusing an unchanged image table's resolution, so the table is resolved again |
| `APS5_NO_TABLE_SNAPSHOT_REUSE` | presence | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` |  | turns off the image table snapshot reuse, so every capture reads and compares each table column |
| `APS5_NO_WINDOW_NARROWING` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvMemory/SpirvModuleSetup.cpp` |  | indexes every slot of the shader's vector window instead of only the slots the program uses |
| `APS5_PROBE` | string | `core/shader/recompiler/Translation/src/DispatchInstructions.cpp` |  | pc:vgpr[:shift] probe that copies that VGPR into v255 after the instruction so image stores write it as a per-lane trace |
| `APS5_RAYTRACING` | string | `core/shader/recompiler/Translation/src/DispatchInstructions.cpp` |  | selects the image_bvh_intersect_ray behaviour: strict throws and miss writes 0xffffffff |
| `APS5_SAVEEXEC_WRITE_FIRST` | presence | `core/shader/recompiler/Translation/src/ControlFlowInstructions.cpp` |  | writes the saved exec to the destination before reading the source in s_*_saveexec |
| `APS5_SINGLE_LANE` | string | `core/shader/recompiler/Recompiler.cpp` |  | hex code addresses (comma separated, or all) of wave64 programs kept at one guest lane per invocation |
| `APS5_SRT_DEBUG` | presence | `core/shader/recompiler/Optimization/src/RequestMemoryView.cpp` |  | prints extra SRT descriptor-walk debug output, such as guest reads outside the request memory |
| `APS5_TRACE_BDA` | presence | `core/shader/recompiler/Optimization/src/ResourceTracker.cpp` |  | prints the first few accesses that make a program take the address-based path |
| `APS5_TRACE_BUFFER_WRITTEN` | presence | `core/shader/recompiler/Optimization/src/DescriptorBindingBuilder.cpp` |  | prints how many guest buffer elements a program may store to and how many it only loads |
| `APS5_TRACE_IMAGE_TABLE` | presence | `core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp` |  | traces image-table resolution |
| `APS5_VERIFY_SRT_COMPACT` | presence | `core/shader/recompiler/Optimization/src/SrtWalker/SrtDescriptorEvaluation.cpp` |  | walks every SRT descriptor a second time through the IR evaluator and throws when the two walks differ |
| `APS5_VERIFY_SRT_TAPE` | presence | `core/shader/recompiler/Optimization/src/SrtWalker/SrtTape.cpp` |  | walks a plan's runtime sources a second time through the evaluator, adopts its answers where the two differ and prints the first twenty differences |
| `APS5_WAVE_LDS_SUBGROUP` | presence | `core/shader/recompiler/SpirvBackend/src/SpirvEmitter.cpp` |  | restores subgroup-scope LDS barriers for single-wave workgroups instead of workgroup scope |

### libSceAgc

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_TRACE_LABEL` | integer | `core/libs/prx/libSceAgc/Cb/src/Sync.cpp` | 0 | hex guest address of a RELEASE_MEM label whose packet is logged when recorded |

### libSceAgcDriver

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_GPU` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | restricts physical device selection to device names containing this case-insensitive substring |
| `ANYPS5_OVERLAY` | string | `core/libs/prx/libSceAgcDriver/Execution/src/PerformanceOverlay.cpp` | 0 | 1 shows the performance overlay; 0 or unset hides it, and any other value is refused |
| `ANYPS5_OVERLAY_POS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/PerformanceOverlay.cpp` | tl | the overlay's corner: tl, tr, bl or br |
| `ANYPS5_PRESENT_MODE` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | fifo | selects the swapchain present mode: fifo, fifo_latest_ready, mailbox or immediate |
| `ANYPS5_REQUIRE_DESCRIPTOR_BINDING_FLAGS` | presence | `core/libs/prx/libSceAgcDriver/tests/ComputePipelineReplay.cpp` |  | fails the replay test instead of accepting a device that refuses descriptor binding flags |
| `ANYPS5_REQUIRE_ROBUST2` | presence | `core/libs/prx/libSceAgcDriver/tests/execution/LdsMixedWidthAtomics.cpp` |  | fails the LDS lock-budget test when the device does not enable robustBufferAccess2, instead of only printing it |
| `ANYPS5_REQUIRE_VULKAN` | presence | `core/libs/prx/libSceAgcDriver/tests/BdaDevice.cpp` |  | fails a test instead of skipping when no usable Vulkan device is present |
| `ANYPS5_SCREENSHOTS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/Screenshot.cpp` |  | directory the screenshot key saves its PNG files to, defaulting to anyps5-screenshots beside the executable |
| `APS5_ADDRESS_COPY_MAX_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 64 | upper bound in MiB on the guest memory one address-based build copies; exceeding it aborts |
| `APS5_ALL_BUFFERS_WRITTEN` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | notes every shader buffer element as written, as before bufferWritten existed |
| `APS5_ASSERT_GPU_LOCK` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | aborts when a guarded path is reached without holding GuestMemory::GpuMutex |
| `APS5_ATOMIC_STAGE_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 1024 | size cap in KiB on an atomic shader element covered by device-local staging |
| `APS5_BARRIER_VALIDATE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | counts the barriers a hazard-tracking validator would emit and skip, by class and hazard kind |
| `APS5_BATCH_CAP` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` | 64 | recorded work since the last submit at which a label batch, flush or reap is forced (0 disables the cap) |
| `APS5_BDA_FLUSH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | also flushes pending storage results over address-based shader regions, which are otherwise exempt |
| `APS5_BDA_STAGING_MIN_THREADS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 512 | smallest dispatch, in threads, whose address-based build stages its written elements device-local; an indirect dispatch always does |
| `APS5_BDA_TABLE_CACHE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/BdaResources.cpp` | 8 | number of recently used BDA page tables kept in the cache (1 is the previous behaviour) |
| `APS5_BLOCK_WRITEBACK_EACH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | stores the touched units only on every write-back instead of widening repeated partial stores |
| `APS5_BLOCK_WRITEBACK_FRAMES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` | 8 | window of presents within which repeated partial stores trigger the whole-image widening |
| `APS5_BLOCK_WRITEBACK_PIECES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` | 4 | number of partial stores within the window that triggers the whole-image widening |
| `APS5_BLOCK_WRITEBACK_WIDEN` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | enables the widening that stores a whole image when it keeps being asked for in pieces |
| `APS5_BUFFER_POOL_SHARED` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/BufferPool.cpp` |  | uses one buffer tier for every allocation size, as before the tier split |
| `APS5_BUFFER_POOL_SLOTS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/BufferPool.cpp` | 4096 | bounds the buffers each pool tier retains (0 keeps the default of 4096) |
| `APS5_CAPTURE_GPU_COPIES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | records GPU buffer copies in the capture trace (requires APS5_CAPTURE_TRACE) |
| `APS5_CAPTURE_INPUTS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | records each draw's inputs in the capture trace (requires APS5_CAPTURE_TRACE) |
| `APS5_CAPTURE_TRACE` | presence | `core/libs/prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp` |  | writes capture events to capture-trace.log |
| `APS5_CHECK_INDIRECT_ARGS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | reads GPU-side indirect draw records back on the CPU and prints the first ones |
| `APS5_COMPLETION_STORE_SYNC` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | makes a store made by a completion action wait for later batches over its range |
| `APS5_COPY_DESTINATION_STRICT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | refuses a CPU copy whose destination has any pending writer, not only an unsettled one |
| `APS5_COPY_HLE_CPU_MAX` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` | 262144 | largest copy in bytes the CPU side of the copy HLE handles |
| `APS5_COPY_HLE_GPU_MAX` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` |  | largest copy in bytes the GPU side of the copy HLE handles; unset means no limit |
| `APS5_COPY_READ_TRACKING` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | notes in-place reads of a range; set to 0 to disable, making the CPU copy path require an idle recorder |
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
| `APS5_DEPTH_PLANE_COPY_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp` | 256 | budget in MiB of the sampled depth-plane copies kept for reuse (least recently used copies leave first); `0` keeps every copy |
| `APS5_DEPTH_PLANE_WHOLE_STORE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp` |  | stores a stale depth plane whole when memory over it is read, instead of only the 64 KiB units the read overlaps |
| `APS5_DISPATCH_CACHE_BYTES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` | 268435456 | size of the dispatch cache in bytes (0 keeps the default of 256 MiB) |
| `APS5_DISPATCH_CACHE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` | 8192 | number of dispatch cache entries; 0 picks a default from the variant count |
| `APS5_DISPATCH_STAMP_VALIDATE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` |  | validates dispatch cache hits by write stamps, forcing one variant and a 4096-entry cache |
| `APS5_DISPATCH_VARIANTS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` | 4 | number of dispatch variants kept per cache entry, clamped to the maximum |
| `APS5_DISPMOVE_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 1024 | upper bound in KiB on the read-only buffers a dispatch-cache hit moves into its template, above which the dispatch is prepared again |
| `APS5_DRAIN_ALL` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketMemory.cpp` |  | drains every packet, disabling the batched recording path |
| `APS5_DRAIN_COMPLETION_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | records completion labels immediately instead of deferring them with the batch, and drains a label recorded while a copied write-back is pending instead of storing it behind the write-backs |
| `APS5_DRAW_CACHE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` | 4096 | number of entries in the draw cache; 0 keeps the default 4096 |
| `APS5_DRAW_DRAIN` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketMemory.cpp` |  | treats draw packets as drains, waiting for them instead of recording ahead |
| `APS5_DRAW_INPUTS_IN_PLACE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | binds draw vertex and index ranges in place from host imports also with NVIDIA's Windows driver, which copies them by default |
| `APS5_DRAW_KEY_ALL_USER_WORDS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawRegisterKey.cpp` |  | includes all 32 user words in a draw's register key instead of the used count |
| `APS5_DRAW_PIPELINE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | runs the second half of a pipelined draw on a draw finisher thread and keeps proofs for the collect epoch |
| `APS5_DRAW_SNAPSHOT_MAX_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 64 | upper bound in MiB on the snapshot bytes a draw's bindings may take |
| `APS5_DRAW_TRANSITIONS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | keeps the per-draw upload and download barriers and layout transitions instead of the lean recorded path |
| `APS5_DRAW_UPLOAD_MAX` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 4096 | largest draw snapshot in bytes copied into the per-recorder upload chunk; 0 turns the chunk off |
| `APS5_DUMP_COMPUTE_MIN_WORDS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | only captures compute pipelines whose SPIR-V has at least this many words |
| `APS5_DUMP_COMPUTE_PIPELINES` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | directory in which captured compute pipelines are written |
| `APS5_DUMP_DRAW_SHADERS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` | 0 | hex color-target or slot-0 address whose draw's shaders are dumped |
| `APS5_DUMP_DRAW_SLOT1` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` | 0 | hex slot-1 address whose draw's shaders and register state are dumped |
| `APS5_DUMP_FRAMES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 0 | saves the first n presented frames as frame_<index>.bmp |
| `APS5_DUMP_FRAMES_EVERY` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 1 | dumps every n-th presented frame |
| `APS5_DUMP_QUEUE` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | dumps the first submissions of the queue with this hex id |
| `APS5_DUMP_REJECTED` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/CommandValidation.cpp` |  | writes a rejected submission to rejected_submission_<n>.bin |
| `APS5_DUMP_SCALE` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 4 | keeps every n-th pixel in each direction of a dumped frame |
| `APS5_DUMP_SHADERS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | writes each prepared shader's request to shader_<address>.req |
| `APS5_DUMP_STORAGE` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | selects storage textures to dump by hex address or WxHxL[:format] extent as storage_<address>_<n>.raw |
| `APS5_DUMP_SUBMITS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/SubmissionCapture.cpp` |  | directory in which every submission is captured for replay |
| `APS5_DUMP_TARGETS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` | 0 | dumps the first n renders of each non-resident color target as target_<address>_<n>.raw in the working directory; a non-zero value also runs every draw synchronously, and in practice it needs APS5_NO_RESIDENT_TARGETS=1 |
| `APS5_DUMP_TEXTURE` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | hex addresses or WxH extents whose sampled textures are dumped on first upload as texture_<address>_<n>.raw |
| `APS5_EAGER_WRITEBACK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | writes back pending storage layers eagerly |
| `APS5_EXACT_DRAW_WRITES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WrittenBuffers.cpp` |  | sizes a color or depth target's draw write ranges from its own bytes and slices instead of the slack and unknown-byte approximation; 0 leaves the approximation on |
| `APS5_FILL_CHAIN` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | seeds and doubles the fill pattern in place in the import |
| `APS5_FILL_CLEAR_EXACT_ONLY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferFill.cpp` |  | turns a fill into a clear only when it covers the target exactly and nothing else |
| `APS5_FILL_SYNC` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | waits for every recorded store over the fill's range |
| `APS5_FLIP_INFLIGHT` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` | 1 | number of presentations that may trail on the GPU (0, 1 or 2) |
| `APS5_FLIP_READ_CHECK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records the in-place reads of completed batches and checks them before the present blit's fence |
| `APS5_FORCE_SDR` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/HostHdr.cpp` | unset | suppresses advertised HDR availability while retaining detected host capabilities; uses the existing SDR presentation path |
| `APS5_FORCE_STRUCTURIZER_FALLBACK` | presence | `core/libs/prx/libSceAgcDriver/tests/ControlFlow.cpp` |  | makes the control-flow test expect the reach-flag fallback instead of no cloning |
| `APS5_FRAME_CAPTURE` | string | `core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp` |  | directory in which the driver records a few frames of the title's driver calls and guest memory for `agc_frame_replay` |
| `APS5_FRAME_CAPTURE_AT_FLIP` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp` | 1 | frame boundary (CPU flip or submission that flips) at which the frame capture starts when no trigger file is set |
| `APS5_FRAME_CAPTURE_FRAMES` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp` | 10 | number of frame boundaries the frame capture records |
| `APS5_FRAME_CAPTURE_TRIGGER` | string | `core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp` |  | file whose appearance starts the frame capture at the next frame boundary; a number in it replaces the frame count |
| `APS5_FULL_BARRIERS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records every barrier instead of merging redundant ones |
| `APS5_GPU_CHECKPOINTS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | attaches device diagnostic checkpoints through VK_NV_device_diagnostic_checkpoints, or buffer markers through VK_AMD_buffer_marker on a device without it |
| `APS5_GPU_COPY_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 1024 | size cap in KiB on a descriptor sub-range copied through the GPU path, larger ones staying on the CPU |
| `APS5_GPU_NO_WRITEBACK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | skips writing GPU results back into guest memory |
| `APS5_GPU_WAIT_TIMEOUT_MS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` | 1000 | milliseconds a GPU wait may run before a warning is printed |
| `APS5_GROUP_CAPTURE_ALL_QUEUES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchAhead.cpp` |  | resolves group-ahead capture on queue 0 as well |
| `APS5_HANDOFF_SPIN_US` | integer | `core/libs/prx/libSceAgcDriver/Execution/include/HandoffSpin.hpp` |  | microseconds a thread spins for a handed-off draw's work or for the GPU mutex before blocking, clamped to 0..10000; unset spins 150 us when the process has more than 8 usable processors and not at all otherwise |
| `APS5_HEAP_MIRROR_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 24576 | budget in MiB for the heap mirrors held for registered allocations |
| `APS5_HOOK_FLUSH_CPU_BLOCKS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | set to 0 flushes pending images whose every 64 KiB block the CPU wrote, instead of skipping them |
| `APS5_HOOK_FULL_SYNC` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | makes the flush hook's recorded-store wait a full Sync under the GpuMutex |
| `APS5_HOOK_LOCKED_WAIT` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | keeps the flush hook's pending-write wait under the GpuMutex |
| `APS5_HOST_IMPORT_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 0 | budget in MiB for pinned host imports; `0` selects the larger of 6144 MiB and half the physical memory, at most the largest host heap minus the larger of 2 GiB and half that heap (4055 MiB on a 16 GB Windows PC with an RTX 4060) |
| `APS5_IGNORE_DEPTH_TEST` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp` |  | renders depth- and stencil-tested draws without a depth target as if the tests always passed |
| `APS5_IMAGE_POOL_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/BufferPool.cpp` |  | size in MiB of the released image memory the buffer pool keeps for reuse by memory type and size while the device-local heap is under its reported budget, a quarter of the device tier's budget by default; `0` frees image memory at once |
| `APS5_INDIRECT_VERTEX_MIB` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/IndirectDraw.cpp` | 64 | cap in MiB on a vertex attribute extent, above which a GPU-side indirect draw resolves on the CPU |
| `APS5_INSERT_COMPARE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/DispatchCache.cpp` |  | compares a fresh capture's regions against the recorder before inserting it into the cache |
| `APS5_KEPT_DESCRIPTOR_SETS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 8 | number of freed dedicated image table descriptor sets kept for a later build with the same layout and sizes; 0 keeps none |
| `APS5_KEYFLIP_DROP` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | drops a pending unit's results on a clear-code to uncompressed key flip, as before |
| `APS5_KEYS_FILL_CLEAR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | clears an image from DCC keys whose fill pattern it matches (only when set to 1) |
| `APS5_LABEL_FLUSH_US` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` | 1000 | age in microseconds at which a pending label batch is recorded or submitted |
| `APS5_LABEL_GATE_ALL` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | gates a label store on any pending completion, not only one overlapping the range |
| `APS5_LABEL_LOCK_EACH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | records each label under the GpuMutex instead of deferring it |
| `APS5_LABEL_OWN_LOCK` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketFlush.cpp` |  | makes a label flush take the GpuMutex itself instead of relying on a self-locking packet |
| `APS5_LABEL_REAP_ALL_QUEUES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | reaps completion labels on every queue instead of only queue 0 |
| `APS5_LABEL_RUNS_INLINE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records store runs at once and closes each run at the next command |
| `APS5_LABEL_STORE_ALWAYS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | writes back every label store even when a later store already covered its bytes |
| `APS5_LABEL_SUBMIT_NOW` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | submits every label immediately instead of waiting for the flush deadline |
| `APS5_LABEL_TRUST_LATE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | trusts cache entries with a stamp at or below the wait's stamp under the late rule (off unless set to a non-zero value) |
| `APS5_LABEL_TRY_EACH_PACKET` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | attempts to record deferred labels at every self-locking packet instead of deferring to its lock |
| `APS5_LAND_WAITS_ALL_QUEUES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | notes pending waits on queue 0 as well |
| `APS5_LAZY_SHADER_PREPARE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | prepares a registered shader on its first use instead of while registering it |
| `APS5_LDS_SLACK` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderInputState.cpp` | 0 | dwords added to every dispatch's LDS allocation |
| `APS5_LOCKED_BUILD` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | builds the whole dispatch preparation under the GpuMutex, skipping the split path |
| `APS5_LOCKED_DRAW_PREPARE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp` |  | takes the GpuMutex before preparing a draw |
| `APS5_LOG_STATS` | presence | `core/libs/prx/libSceAgcDriver/Execution/include/AsyncLog.hpp` |  | prints the AGC log's line, byte and coalescing statistics every window; 0 turns them off |
| `APS5_LOG_SYNC` | presence | `core/libs/prx/libSceAgcDriver/Execution/include/AsyncLog.hpp` |  | writes AGC log lines on the calling thread instead of through the asynchronous sink thread |
| `APS5_LOOP_GUARD` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | enables the fault buffer that guards shader loops running past their bound |
| `APS5_MIMALLOC` | presence | `core/relinker/elfpatcher/src/windows/WindowsEntryStubBuilder.cpp` |  | loads `mimalloc.dll` in the Windows entry stub, so the process allocates through mimalloc instead of the C runtime's heap |
| `APS5_MIRROR_REFRESH_THREADS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 3 | number of helper threads that refresh heap mirrors, clamped to 0 through 16 |
| `APS5_NO_ADDRESS_READ_PASSES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | stops a draw that only reads through its buffer descriptors from counting as read-only, so it breaks the pass instead of continuing the producer's |
| `APS5_NO_ADDRESS_SPACE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | disables the address-space cache so every build rebuilds its space |
| `APS5_NO_ADJACENT_GENERATION` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | leaves adjacent pending images at their generation during a write-back, as before |
| `APS5_NO_ALIAS_BORROW` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | disables borrowing a pending aliased storage image |
| `APS5_NO_ATOMIC_STAGING` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | binds atomic shader elements in place instead of staging them through a device-local buffer |
| `APS5_NO_BARRIER_BATCH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records the staging, write-back and label-run leading barriers that a covering barrier already orders, instead of leaving them out |
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
| `APS5_NO_LABEL_PACKET_FLUSH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketFlush.cpp` |  | disables the label flush between packets |
| `APS5_NO_LABEL_READBACK_STORE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | leaves pending linear and small storage images on the GPU at a label instead of storing them before it |
| `APS5_NO_LABEL_RUNS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records every store in a run of its own between a barrier pair |
| `APS5_NO_LABEL_SHORTCUT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | disables the pending-label table shortcut in a memory wait |
| `APS5_NO_LAYER_CLEAR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | tracks every surface as one layer instead of per array layer |
| `APS5_NO_LAYER_REFUSAL_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | re-checks a layer whose surface check failed at its current generation on every fill |
| `APS5_NO_LAYOUT_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | builds descriptor set layouts fresh instead of through the descriptor cache |
| `APS5_NO_LEASE_MIRROR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | restores copies instead of image mirrors for address-based builds |
| `APS5_NO_LIBRARY_PREBUILD` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | disables prebuilding registration-form shader libraries (only 1 disables it) |
| `APS5_NO_LOCK_PROFILE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | turns off GpuMutex wait profiling under APS5_PROFILE_DRAW |
| `APS5_NO_MOVED_BUFFER_TEMPLATES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | keys the full descriptor words of read-only guest buffers instead of allowing a hit when they move |
| `APS5_NO_OBSERVATION_GUARD` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` |  | observes every validated range, not only the ranges whose flush hook waited |
| `APS5_NO_OPPORTUNISTIC_REAP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | skips retiring already-finished batches before a submit |
| `APS5_NO_OWN_IMAGE_REFRESH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | lets a Pending failure of the fast proof take the full walk instead of the own-object refresh |
| `APS5_NO_PARALLEL_MIRROR_REFRESH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | disables the mirror-refresh thread pool, so mirror refreshes run on the calling thread |
| `APS5_NO_PATTERN_FILL_HLE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferFill.cpp` |  | disables the dword-pattern fill HLE |
| `APS5_NO_PAUSE_SPIN` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | disables the CPU pause-spin in a blocked memory wait |
| `APS5_NO_PENDING_HTILE_FILLS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp` |  | reads the HTILE words of a depth target bind even when recorded fills are pending over them, instead of working the words out from the fills |
| `APS5_NO_PERF_FRONTEND_PAIR` | presence | `core/libs/prx/libSceAgcDriver/tests/ShaderMemory.cpp` |  | disables frontend-pair reuse in the shader-memory test |
| `APS5_NO_PERF_LAYOUT_PAIR` | presence | `core/libs/prx/libSceAgcDriver/tests/ShaderMemory.cpp` |  | disables layout-pair reuse in the shader-memory test |
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
| `APS5_NO_RECORDED_CLEAR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | waits for a storage image's fast clear in a batch of its own instead of recording it |
| `APS5_NO_RECORDER_SYNC_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | gives the draws the recorder waits for their own batch instead of recording them with the wait |
| `APS5_NO_RECORD_COPIED_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | waits for copied-write draws at once instead of listing their write-back for later |
| `APS5_NO_RECT_INDIRECT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/IndirectDraw.cpp` |  | sends an indirect rectangle-list draw down the CPU path instead of the GPU-side path |
| `APS5_NO_RESIDENT_LINEAR` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | drops the residency of a linear color target whose address already has a host import, so it is copied in and out again |
| `APS5_NO_RESIDENT_PRESENT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | always presents through guest memory instead of the resident image |
| `APS5_NO_RESIDENT_TARGETS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | copies every render target in and out again instead of keeping it resident |
| `APS5_NO_RESOURCE_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | disables the shared resource cache, so every dispatch builds its resources fresh |
| `APS5_NO_RUN_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | walks the lease for a host import's run of ranges on every lookup instead of reusing the bounds found for the same lease snapshot and import window block |
| `APS5_NO_SAMPLED_FROM_STORAGE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | keeps every sampled texture a CPU snapshot instead of viewing the surface's storage image |
| `APS5_NO_SAMPLER_CACHE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | creates a sampler per binding instead of caching them |
| `APS5_NO_SEPARATE_QUEUED_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | lets a queued label enter the recorded table and replace a recorded entry of its dword |
| `APS5_NO_SINGLE_PASS_COLLECT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | collects written pages in one cleared pass instead of a second confirming walk |
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
| `APS5_PACKET_EPOCH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitStatistics.cpp` |  | makes queue workers bump the collect epoch before every packet |
| `APS5_PIPELINED_DRAWS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawPipeline.cpp` | 8 | depth of the queue-0 draw pipeline: 0 records draws inline, 1 selects the default depth of 8, and any other value is the depth capped at 256 |
| `APS5_PIPELINE_DISPATCH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | records an eligible dispatch as an item in the queue-0 pipeline instead of running it inline |
| `APS5_PIPELINE_DMA` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketMemory.cpp` |  | records a queue-0 DMA data packet in the draw pipeline instead of executing it inline |
| `APS5_PIPELINE_DRAIN_FILLS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferFill.cpp` |  | runs queue-0 buffer fills through the draw pipeline to keep their order; any value stores them in place instead |
| `APS5_PIPELINE_DRAIN_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | answers a queue-0 wait from the draw pipeline's own pending labels while it is busy; any value drains the pipeline at the wait |
| `APS5_PIPELINE_DRAIN_SUBMISSIONS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | lets a submission end without submitting its recorded work, so it joins a later submit with the pipeline's queue; any value drains the pipeline and submits at the end of the submission |
| `APS5_PIPELINE_JOURNAL_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/PipelineJournal.cpp` | 1024 | number of journal entries loaded at most; a longer journal is truncated to this count |
| `APS5_PIPELINE_JOURNAL_MAX_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/PipelineJournal.cpp` | 1024 | size cap in MiB on the pipeline journal; later pipelines are not recorded once it is reached |
| `APS5_PIPELINE_JOURNAL_THREADS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/PipelineJournal.cpp` |  | threads that replay the journal's pipelines, 0 disables the replay; unset uses half the hardware threads, at least one |
| `APS5_PIPELINE_STATS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | captures and prints driver statistics for each new pipeline, requiring `VK_KHR_pipeline_executable_properties`; `1` enables |
| `APS5_PIPELINE_STORE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Pipeline.cpp` | 1024 | LRU bound on the entries of the pipeline store; an entry is evicted only once no recorded draw holds it |
| `APS5_PM4_RING` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/SubmissionRing.cpp` | 16 | number of submissions the PM4 ring keeps for dumps, from 0 to 4096; 0 disables the ring |
| `APS5_POLL_REAP` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | reaps completions on every poll tick |
| `APS5_POLL_TRY_EACH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | tries the poll service on every tick instead of at the poll interval |
| `APS5_PREBUILD_THREADS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` | 1 | number of threads in the one-thread pool that prebuilds registration-form shader libraries |
| `APS5_PRESENT_POLL_FENCE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | polls the present fence every 50 us instead of blocking on it |
| `APS5_PROBE_DISPATCH` | string | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | probes one dispatch at a hex program address (optionally `:index`) with debug output |
| `APS5_PROFILE_DRAW` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Dispatch/Dispatch.cpp` |  | prints the per-phase driver and draw timing totals |
| `APS5_PROFILE_GPU` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | records the GPU time of recorded work by key from batch timestamps |
| `APS5_REAP_EACH_PACKET` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/DeferredLabels.cpp` |  | reaps deferred labels after every packet |
| `APS5_RELEASE_ON_UNLOCK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | destroys deferred batches on the unlocking thread instead of a release thread |
| `APS5_RELEASE_QUEUE_MAX` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` | 256 | maximum deferred batches handed to the release thread before a thread destroys its own |
| `APS5_RELEASE_UNDER_LOCK` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | destroys deferred batches in finish() under the lock instead of on a release thread |
| `APS5_RESOURCE_CACHE_ENTRIES` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 1024 | maximum entries in the per-device shader-resource cache |
| `APS5_RESOURCE_CACHE_TABLE_ELEMENTS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` | 65536 | number of image-table elements the shader-resource cache keeps (0 keeps the default) |
| `APS5_SAMPLE_COMMITTER` | string | `core/libs/prx/libSceAgcDriver/Execution/src/WorkerSampler.cpp` |  | samples queue 0's draw committer every millisecond and writes its stacks to a file (Windows only) |
| `APS5_SAMPLE_THREADS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/WorkerSampler.cpp` |  | samples every thread of the process every 2 ms and writes each busy thread's hottest frames to a file (Windows only) |
| `APS5_SAMPLE_WORKER` | string | `core/libs/prx/libSceAgcDriver/Execution/src/WorkerSampler.cpp` |  | samples the calling queue worker every millisecond and writes its stacks to a file (Windows only) |
| `APS5_SHADOW_VERIFY` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/UnitShadow.cpp` |  | verifies that the shadow-published bytes equal the guest bytes |
| `APS5_SHARED_COLLECT_MEMO` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | uses the tracker's shared collect-memo ring under its mutex instead of the per-thread ring |
| `APS5_SHARED_MEMORY_COUNTERS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | accumulates the memory counters into one shared set with atomic RMWs |
| `APS5_SKIP_INVALID_INDIRECT_REGISTERS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | skips the entries whose offset dword is no register offset in a SET_*_REG_INDIRECT list read again where its packet executes, and writes its valid entries, instead of stopping |
| `APS5_SKIP_REFUSED_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Diagnostics.cpp` |  | skips a draw whose graphics stage the recompiler refuses, reporting each refused shader once, instead of failing the queue |
| `APS5_SKIP_REQUEST_DUMPS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderDiagnostics.cpp` | 8 | caps RecompileRequest dumps per shader-skip reason |
| `APS5_SKIP_SHADER` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Diagnostics.cpp` | 0 | hex program address of a shader to skip preparing |
| `APS5_SNAPSHOT_CHECK_FATAL` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | throws on the first snapshot that differs from registered memory instead of only counting them |
| `APS5_SPANNED_WAVE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | spans a single-wave wave64 compute program over two full 32-lane subgroups |
| `APS5_SPIRV_OPT` | string | `core/libs/prx/libSceAgcDriver/tests/SpirvOptimizer.cpp` |  | selects the SPIR-V optimization pipeline: `full` restores the performance pipeline, `none` skips optimization |
| `APS5_SRGB_ATTACHMENT_PROXY` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | forces the sRGB color-attachment proxy format; `1` forces it |
| `APS5_SRGB_SHADER_DECODE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp` |  | `1` forces shader sRGB decode for the sRGB 8-bit formats; `copy` does the same, except that 8 and 8_8 sRGB sample an RGBA8 sRGB copy when the device can blit to RGBA8 |
| `APS5_STAGING_POOL_MIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/BufferPool.cpp` |  | size in MiB of the device-local staging buffer pool; `0` disables the device tier |
| `APS5_STALE_EVICT` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | also evicts a proved-present dead image's pending layers on an access |
| `APS5_STALL_WATCHDOG_SECS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/SubmissionRing.cpp` | 0 | seconds a queue may go without progress before the submission rings are dumped; 0 disables the watchdog, at most 86400 |
| `APS5_STORE_AT_FLIP` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | stores every pending storage-image result at each display flip; only `1` is accepted |
| `APS5_STORE_AT_LABEL` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | flushes every pending storage-image result when a label is written on the GPU |
| `APS5_SUBMISSION_EPOCH_EACH` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | bumps the guest-memory collect epoch on every submission instead of only when the submission is newer than the last bump |
| `APS5_SUBMIT_AT_END` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketExecution.cpp` |  | submits recorded work at the end of a submission even when a worker is queued |
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
| `APS5_THREAD_PRIORITY` | string | `core/libs/prx/libSceAgcDriver/Execution/src/ThreadPriority.cpp` | off | raises the queue workers and presenter above the title's threads: `off`, `high` or `rt` |
| `APS5_THREAD_RT` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/ThreadPriority.cpp` | 10 | realtime priority the raised threads ask rtkit for (clamped to 1..99) |
| `APS5_THREAD_RTTIME_US` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/ThreadPriority.cpp` | 200000 | hard RLIMIT_RTTIME the realtime plan lowers to, in microseconds |
| `APS5_TRACE_BARRIERS` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp` |  | prints that many simulated hazard barriers before stopping |
| `APS5_TRACE_BENCH` | string | `core/libs/prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp` |  | `1` with `APS5_TRACE_SUMMARY_EVERY` also rewrites `aps5-bench-<commit>-<pid>.json.gz` with every interval's frame times; any other value fails the first present |
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
| `APS5_TRACE_EVENTS` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/PerformanceTrace.cpp` |  | number of events a windowed trace keeps in its buffer |
| `APS5_TRACE_FILL_COVER` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | prints the first fills that meet images without covering them |
| `APS5_TRACE_FLUSH` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp` |  | names what forces pending storage-image results into guest memory |
| `APS5_TRACE_FRAMES` | string | `core/libs/prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp` |  | records `<count>` presents from present `<start>` (whole numbers, `start:count`) |
| `APS5_TRACE_GPU` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/DisplayBuffer.cpp` |  | prints GPU wait, submission and scanout events, the execute lines, and waits longer than 200 ms (read in DisplayBuffer, PacketExecution, QueueWorker, Submission and WaitMemory) |
| `APS5_TRACE_HOST_IMPORT` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | traces host-import mapping failures |
| `APS5_TRACE_INDIRECT_DRAWS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawPrecheck.cpp` |  | prints indirect draw packets that reach the precheck |
| `APS5_TRACE_LATE_LABELS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | prints how late labels were trusted or refused |
| `APS5_TRACE_PREPARATION_WAIT` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | prints where a demanded shader preparation sat in the queue |
| `APS5_TRACE_QUERY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp` |  | prints every 16th page query with its cost |
| `APS5_TRACE_RESOURCE_KEY_MISSES` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | reports shader-resource cache key misses |
| `APS5_TRACE_SECONDS` | string | `core/libs/prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp` |  | records from the first present `<start>` seconds after the first present for `<length>` seconds (whole numbers, `start:length`) |
| `APS5_TRACE_SHADER_REGS` | string | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp` |  | prints a shader's registers for one hex program address, or `all` |
| `APS5_TRACE_SHADOW` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/UnitShadow.cpp` |  | prints the first n shadow publishes, the first 64 seeds, every evict and retire |
| `APS5_TRACE_SKIPS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderDiagnostics.cpp` |  | reports each distinct shader skip once |
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
| `APS5_VERIFY_DRAW_PIPELINE` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | runs a full check next to 1 in 64 reused proofs and prints `[epoch-proofs]` counts |
| `APS5_VERIFY_DRAW_RECIPE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCache.cpp` |  | re-decodes a draw recipe instead of using the cache and checks the result |
| `APS5_VERIFY_DRAW_SNAPSHOTS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | re-reads a reused draw snapshot and counts the ones that differ |
| `APS5_VERIFY_KNOWN_VALUES` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/BufferCopy.cpp` |  | serves known values through the hook too and counts mismatches |
| `APS5_VERIFY_PROOFS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp` |  | runs the full walk beside every own-object refresh proof and aborts on a different decision |
| `APS5_VERIFY_RECIPE` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | makes the ordinary prepare path beside a recipe hit and compares the answers |
| `APS5_VERIFY_REGISTER_KEYS` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawRegisterKey.cpp` |  | checks the incremental register fingerprint against the full state |
| `APS5_VERIFY_SET_LAYOUTS` | presence | `core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp` |  | checks the draw's set layouts against those described from the shaders |
| `APS5_WAIT_LOCK` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | always takes the GPU mutex on a wait (no unlocked label lookup) |
| `APS5_WAIT_LOCK_ENTRY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Synchronization/WaitMemory.cpp` |  | takes the GPU mutex blocking on the entry instead of trying it |
| `APS5_WATCH_MEMORY` | string | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | watches named guest ranges (`<hex address>:<hex bytes>[,...]`) across dispatches; needs `APS5_SYNC_DISPATCH=1` |
| `APS5_WORKER_AFFINITY` | presence | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/WorkerAffinity.cpp` |  | pins the queue workers and presenter to the performant cores (needed to enable it on non-Linux hosts) |
| `APS5_WORKER_AFFINITY_MASK` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/WorkerAffinity.cpp` |  | explicit CPU affinity mask for the queue workers and presenter |
| `APS5_WORKING_SET_MIB` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp` |  | sets the process working-set minimum to this many MiB (Windows only) |
| `APS5_WRITE_EVIDENCE_AFTER` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` | 4 | consecutive unchanged observations before a dword's written-element evidence is trusted |
| `APS5_WRITE_EVIDENCE_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` | 16 | largest written range in KiB that the evidence may serve |
| `APS5_WRITE_EVIDENCE_SAMPLE` | integer | `core/libs/prx/libSceAgcDriver/Execution/src/Driver/Memory/WriteEvidence.cpp` | 16 | reads one in this many pending writes to sample them |
| `APS5_WRITE_WATCH_IMPORTS` | string | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` |  | how host imports are write-tracked: `probe`, `watch` or `unwatch` |
| `APS5_WRITTEN_SHADOW_MAX_KIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 2048 (8192 on a discrete GPU) | upper bound in KiB of the written staging window; a set value is used as given on both device types |
| `APS5_WRITTEN_SHADOW_MIN_KIB` | integer | `core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp` | 16 | lower bound in KiB of the written staging window |

### libSceAjm.native

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_TRACE_AJM` | presence | `core/libs/prx/libSceAjm.native/src/Ajm.cpp` |  | logs every job the title submits and what the library writes back |

### libSceAudioOut

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
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
| `APS5_PAD_INPUT_FILE` | string | `core/libs/prx/libScePad/src/PadState.cpp` |  | path of the file polled for injected pad button and stick commands |

### libScePlayGo

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_PLAYGO_UNLISTED_CHUNKS` | string | `core/libs/prx/libScePlayGo/Export.cpp` |  | answer for a chunk ID the playgo manifest does not list: installed or absent; unset throws instead of answering |

### libSceSaveData.native

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_SAVEDATA_TRACE` | presence | `core/libs/prx/libSceSaveData.native/Export.cpp` | 3 | logs every save-data operation instead of the first three |

### libSceSystemService

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_LANGUAGE` | integer | `core/libs/prx/libSceSystemService/Export.cpp` | 1 | console language number returned by sceSystemServiceParamGetInt, defaulting to English US |

### libSceVideoOut

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_INPUT_CONFIG` | string | `core/libs/prx/libSceVideoOut/src/InputMapping.cpp` |  | path of the input mapping file that binds host keys and mouse to guest pad actions |
| `ANYPS5_REQUIRE_DISPLAY` | presence | `core/libs/prx/libSceVideoOut/tests/Flip.cpp` |  | makes video-out tests fail instead of skipping when no display or Vulkan device is available |
| `APS5_NO_PAD_INPUT` | presence | `core/libs/prx/libSceVideoOut/src/PadInput.cpp` |  | ignores keyboard and mouse input so a measurement run is not advanced by a stray press |
| `APS5_VIDEOOUT_TELEMETRY_DIR` | string | `core/libs/prx/libSceVideoOut/src/Telemetry.cpp` |  | directory where videoout.jsonl is appended with the pad and present telemetry lines; nothing is opened when unset or empty |

### libSceVideodec2

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_TRACE_VIDEODEC` | presence | `core/libs/prx/libSceVideodec2/Export.cpp` |  | logs decoder memory queries, queue allocation and the first few decodes |

### libc

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `APS5_CRASH_INDIRECT_MEMORY` | presence | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | dumps the words the faulting registers point at in the Windows crash report |
| `APS5_CRASH_STACK_BYTES` | integer | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` | 0 | bytes of the faulting thread's stack from rsp, decimal or 0x hexadecimal and at most 1048576, that the Windows crash report prints with the data at the stack words pointing outside the stack; 0 prints none |
| `APS5_HANG_DUMP_SECS` | integer | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | seconds after which a watchdog dumps all threads and aborts |
| `APS5_HEAP_CACHE_MIB` | integer | `core/libs/prx/libc/src/GuestHeap.cpp` | 4096 | size in MiB of the cache of freed large blocks before the oldest are decommitted |
| `APS5_HELPER_AFFINITY_MASK` | integer | `core/libs/prx/libc/include/CpuTopology.hpp` |  | hex core mask that helper threads (deferred release, mirror refresh) are pinned to |
| `APS5_LOG_TIMESTAMPS` | presence | `core/libs/prx/libc/include/general/LogMacros.hpp` |  | prefixes each log line with the milliseconds since the first log |
| `APS5_NO_PROCMAP_QUERY` | presence | `core/libs/prx/libc/include/specifics/linux/HostMapping.hpp` |  | skips the /proc/self/maps PROCMAP_QUERY ioctl and scans the maps file instead |
| `APS5_NO_SSE4A_EMULATION` | presence | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | disables emulation of AMD-only SSE4a instructions, making such a fault fatal |
| `APS5_NO_WRITE_WATCH` | presence | `core/libs/prx/libc/src/GuestArena.cpp` |  | disables write watching so guest buffers are fully re-read on every use |
| `APS5_PIN_WAIT_MS` | integer | `core/libs/prx/libc/src/GuestAllocations.cpp` | 60000 | milliseconds to wait for a pinned allocation lease before failing |
| `APS5_TRACE_AFFINITY` | presence | `core/libs/prx/libc/include/CpuTopology.hpp` |  | prints a line naming the role, tid and mask of every thread pinning |
| `APS5_TRACE_CALLS` | string | `core/libs/prx/libc/src/specifics/x86_64/CallTrace.cpp` |  | comma-separated libraries (or all) whose guest calls are traced |
| `APS5_TRACE_EXIT` | presence | `core/libs/prx/libc/src/Process.cpp` |  | prints a backtrace of the caller of exit, _Exit and the process-exit path |
| `APS5_TRACE_SSE4A` | presence | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | prints each emulated SSE4a instruction once per rip |
| `APS5_WATCH_ADDR` | integer | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | absolute hex address whose writes are reported after its page is committed |
| `APS5_WATCH_WRITE` | integer | `core/libs/prx/libc/src/specifics/windows/CrashReport.cpp` |  | guest ELF vaddr whose 8-byte writes are reported by single-stepping the protected page |

### libkernel

| Name | Kind | Source | Default | Description |
|------|------|--------|---------|-------------|
| `ANYPS5_LOADEXEC_ARGV_SKIP` | string | `core/libs/prx/libkernel/System/src/Process.cpp` |  | on Windows, set to `1` to drop the first process argument and clear the variable |
| `APS5_COARSE_TIMED_WAITS` | presence | `core/libs/prx/libkernel/Time/TimedWait.cpp` |  | on Windows, rounds timed waits up to the scheduler tick |
| `APS5_JOB_AFFINITY` | presence | `core/libs/prx/libkernel/Pthread/src/Thread.cpp` |  | pins the title's job worker threads to the efficiency cores of a hybrid CPU |
| `APS5_JOB_AFFINITY_MASK` | integer | `core/libs/prx/libkernel/Pthread/src/Thread.cpp` |  | hex core mask that job worker threads are pinned to |
| `APS5_NO_DIRECT_MEMORY_SECTION_POOL` | presence | `core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp` |  | on Windows, creates a section for every direct-memory allocation and closes it at release instead of reusing released sections that never held guest data |
| `APS5_NO_JOB_AFFINITY` | presence | `core/libs/prx/libkernel/Pthread/src/Thread.cpp` |  | disables pinning of job worker threads |
| `APS5_NO_SLEEP_WAKE` | presence | `core/libs/prx/libkernel/Time/SleepWake.cpp` |  | keeps every sleep of 1 ms or less in full instead of ending it at a GPU batch, batch completion action or flip completion |
| `APS5_NO_TIMER_RESOLUTION` | presence | `core/libs/prx/libkernel/Time/Time.cpp` |  | keeps the default 15.6 ms scheduler tick instead of raising it to 0.5 ms on Windows |
| `APS5_TIME_SCALE` | integer | `core/libs/prx/libkernel/Time/Time.cpp` | 1 | factor applied to the guest monotonic clocks and TSC; sleeps and host JobManager worker spin intervals still take real time |
| `APS5_TRACE_APR` | presence | `core/libs/prx/libkernel/Apr/src/Apr.cpp` |  | logs every APR file read with its path, offset, size and destination |
| `APS5_TRACE_MEMORY` | presence | `core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp` |  | logs direct-memory map, remap, protect, unmap and physical allocation operations |
| `APS5_TRACE_SYNC` | presence | `core/libs/prx/libkernel/EventFlag/src/EventFlag.cpp` |  | logs event-flag create, set and wait operations |
| `APS5_TRACE_TIMERS` | presence | `core/libs/prx/libkernel/Time/Time.cpp` |  | runs a timed-wait selftest at startup and prints mean, p50 and max latencies |
| `APS5_TRACE_USLEEP` | presence | `core/libs/prx/libkernel/Time/Time.cpp` |  | reports every 2000 usleep calls the guest call sites that sleep most |
| `APS5_TRACE_WAITS` | presence | `core/libs/prx/libkernel/Time/Time.cpp` |  | aggregates blocking waits per guest call site and prints the longest-waiting sites every 3 s |

