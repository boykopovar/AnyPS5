# libSceVideoOut

## change [Windows game controllers]

- [Windows game controllers](../../../core/libs/prx/libSceVideoOut/src/VideoOutDriver.cpp) (libSceVideoOut) - SDL's DirectInput backend is disabled at default hint priority before `SDL_InitSubSystem` because enumeration hangs on some vendor HID devices (#1042); XInput/HIDAPI work, while DirectInput-only devices need `SDL_DIRECTINPUT_ENABLED=1` (unsafe with those devices). SDL's Windows.Gaming.Input backend stays enabled by default; `APS5_NO_WGI` disables it at default priority for hosts crashing at `Windows.Gaming.Input.dll+0x10a84` (observed on Win10 22H2 19045 with Nefarius/Oculus virtual gamepad buses and no physical controller; the fault happens a few seconds into every run), dropping WGI-only wheels, arcade/flight sticks, non-HIDAPI HID pads, and Xbox One/Series trigger rumble.

# libc

## change [sceLibcMspaceReallocalign]

- [sceLibcMspaceReallocalign](../../../core/libs/prx/libc/src/Mspace.cpp) (libSceLibcInternal, in libc) takes `(heap, block, alignment, size)`, matching console `libSceLibcInternal.sprx` (SDK 4.5x `J03950001` `0x13880`). A null block goes to the routine `sceLibcMspaceMemalign` uses, from the internal routine at `0x12390`. Measured on PS5 FW 13.60 payload: alignment 4096 + size 100 gives 4096-aligned; alignment 0 or 3 succeeds and alignment 100 gives a 128 byte aligned block (raised to >= 32, then next power of 2); alignment >= 2^63 returns null leaving block and `errno` untouched; size 0 returns a block (reusing if suitable) instead of freeing. Aligned, large-enough blocks stay in place without shrinking trailing bytes as console does.

## change [getifaddrs]

- [getifaddrs](../../../core/libs/prx/libc/src/Interfaces.cpp) (libc) lists host interfaces and IPv4/IPv6 addresses in FreeBSD layouts. On Windows, `ifi_collisions=0`, `ifi_baudrate` is transmit link speed, multicast counters include broadcasts, and flags are limited to `IFF_UP`/`RUNNING`/`LOOPBACK`/`BROADCAST`/`MULTICAST` (PPP/tunnel omit broadcast/point-to-point; unspecified prefix gets host mask). Across Linux/macOS/Windows, `AF_LINK` maps non-Ethernet/loopback/PPP to `IFT_OTHER` (1); Linux `if_data` zeroes `ifi_omcasts`, `ifi_noproto`, `ifi_hwassist`, `ifi_metric`, `ifi_epoch`, `ifi_lastchange`, and unknown baud, and only address entries have null `ifa_data`; macOS wraps 32-bit counters and zeroes `ifi_oqdrops`/`hwassist`/`epoch`/`lastchange`; peerless point-to-point drops `IFF_POINTOPOINT`; omitted IPv4 broadcast derives from netmask. Interface names are the host's (`if<index>` on Windows > 15B).

# libkernel

## change [Mapping hints with no room above them]

- [Mapping hints with no room above them](../../../core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp) (libkernel) - non-`MAP_FIXED` mappings whose hint is above the guest address space or lacks free room at/above it fall back to the bottom of the guest arena, mirroring FreeBSD 11.4 [`vm_mmap_object`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/vm/vm_mmap.c#L1569) / [`vm_map_find_min`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/vm/vm_map.c#L1555-L1584). FreeBSD starts from the max data-segment end rather than arena base, and console layout is unmeasured. PS5CEMU-HAR (`PPSA99360`) passes `sceKernelReserveVirtualRange` a hint near its code (a host address outside the guest arena here), placing its JIT > 2 GiB from code so it fails to start.

## change [sceKernelCreateEqueue and sceKernelGetKqueueFromEqueue]

- [sceKernelCreateEqueue and sceKernelGetKqueueFromEqueue](../../../core/libs/prx/libkernel/Equeue/Equeue.cpp) (libkernel) - an equeue wraps a kqueue fd as in FW 13.42 `libkernel_sys.sprx` (creates kqueue + named object, returns `(id << 32) | fd`, waits via `kevent`, deletes named object on close; disassembly, unrun on console). Here `id` is a counter, not a named object; pinning rejects mismatched ids so stale recycled-fd handles return `EBADF` across event/wait/delete/file calls. `kevent` on an equeue fd uses the `sceKernel*Equeue` loop while `sceKernelWaitEqueue` on a `kqueue` fd uses the kevent loop, whereas console has one queue (cross-caller behavior unknown). Null `eq` returns `EINVAL` (module writes through it). PS5 FW 11.60 payload (2026-11-07) rejected names >= 32 chars with `ENAMETOOLONG` leaving output unset and accepted 0/9/31 chars, as here; title process unmeasured.

## change [sceKernelRaiseException](../../../core/libs/prx/libkernel/System/src/Exception.cpp) on Linux and macOS sends the host SIGUSR1

- [sceKernelRaiseException](../../../core/libs/prx/libkernel/System/src/Exception.cpp) on Linux/macOS sends host `SIGUSR1` to the target thread, running the handler wherever interrupted (including emulator code). On console, kernel objects are syscalls and libthr defers signals under internal locks; here a handler needing a lock held by interrupted code deadlocks, and a stalling handler holds host locks. Threads holding the address-wait lock (`sceKernelSyncOnAddressWait`, `sceKernelSyncOnAddressWake`, `_umtx_op`) defer the signal until unlock or wait; other emulator locks are not deferred. Host waits resume afterward, except Linux non-restarted calls: non-retrying sleeps return early, and `sceNetEpollWait` poll / timed socket calls return `EINTR` to the guest.

# shader

## change [Color comparison descriptor forms]

- [Color comparison descriptor forms](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp): for float-sampleable formats in GFX10 codes 1–77, `image_sample_c*` returns 0 for all comparison functions except codes 7 (`16_UNORM`) and 22 (`32_FLOAT`), which use native comparison. Split measured with point sampling on RDNA2 (`gfx1031`, `gfx1036`) and point/bilinear on RDNA3 (`gfx1100`); PS5 `gfx1013` unverified. On `gfx1036`, cube comparisons of codes 1/7/22/56 across 8 functions and array comparisons of all 25 codes over never/always/greater match this split (1 and 56 return 0 across all 8 functions on `2D_ARRAY`). Codes 128–130 (8-bit sRGB) also return 0 on `gfx1035` (references `-0.5..1.5`, texels `0..255`). Scaled, integer, and converted-sampling formats remain unsupported (tested hardware also returns 0); anisotropic comparison is unmeasured.

## change [64-bit image atomics]

- [64-bit image atomics](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ImageInstructions.cpp) (`image_atomic_*`, 2-dword mask) without `VK_EXT_shader_image_atomic_int64` (MoltenVK, Intel's Windows driver) are [emulated](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) via a `Coherent` `R32G32_UINT` view declared `Rg32ui` under 2^20 device-wide lock words hashed from texel coordinates. MoltenVK needs MSL 3.2 (macOS 15), lowering device barriers to fences. A swap's write depends on its read (on Apple M3 Max an independent texture write landed before the prior read); 2,048 contended `add`/`umax` on one texel were exact there and on Intel UHD (`8086:a7a8`, 101.7084; throughput unmeasured). Concurrent shaders do not take the lock; 64-bit `inc`/`dec` take it (unmeasured). Other image accesses in the same shader and float atomics throw.

## change On a host subgroup narrower than the wave, [`v_readlane_b32`]

- On a host subgroup narrower than the wave, [`v_readlane_b32`](../../../core/shader/recompiler/SpirvBackend/src/SpirvAlu/SpirvAluEmitterMath.cpp) of a constant out-of-range lane builds only as lane 63 of a half-wave scan (DPP `row_shr` 1/2/4/8, `v_permlanex16_b32`, `EXEC -1`) or, in all-lane entry `EXEC` stages (vertex/local/tessellation), as 0 when lanes 32–63 are 0 (from 0 via selects under a lane index below at most 32/`v_mbcnt(-1)`, `v_cmp_ne_u32` vs 0, OR, DPP, permlanes; the counts in `s3` give such a bound on a 32-lane host); other reads throw. On 32-lane hosts a half-wave scan may also use `s_mov_b64 exec, -1`, `bound_ctrl:1` steps for OR/add/umax with 0 in lanes 32–63, making lane 31 the host subgroup reduction: idle lanes add identity where the console adds the keys of the lanes `s3` claims.

## change [Mesh shader grid size]

- [Mesh shader grid size](../../../core/shader/recompiler/SpirvBackend/src/SpirvModuleEmitter.cpp) (shader recompiler) - every mesh shader loads `NumWorkgroups.x` and declares no vertices when 0 (never produced by dispatches). Workaround for RADV (Mesa 26.2.4 on RX 9070 XT, GFX12): binding a mesh shader reading neither `NumWorkgroups` nor `DrawIndex` keeps the user SGPR count and draw-id flag of the vertex pipeline bound before it in the same command buffer (`radv_bind_pre_rast_shader` updates only with `AC_UD_VS_BASE_VERTEX_START_INSTANCE`), so `radv_emit_userdata_mesh` opens a 3-register `SET_SH_REG` and writes one value, causing CP to read `DISPATCH_MESH_DIRECT` as register data and hang the GPU (gfx ring timeout, MODE1 reset). *Kena: Bridge of Spirits* (`PPSA01802`) hits this on layered volume draws. Reading `NumWorkgroups` ensures the grid-size slot exists; drop once fixed Mesa is minimum.
