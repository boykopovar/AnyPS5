# libSceFaceTracker technical debt

### Unknown function info

- [Additional libSceFaceTracker exports](../../../core/libs/prx/libSceFaceTracker/Export.cpp) - the 10 name/NID pairs below are defined function exports recovered from the GTAVC-bundled `libSceFaceTracker.prx` export table (the 4.03 firmware dump does not ship this module; the GTAVC bundle is the only concrete binary evidence). Their signatures and behavior are unknown; parameterless `int` declarations are import placeholders, not verified ABI prototypes. Every call throws through `NotImplemented_nid_no_patch`. No tracking, update, or result behavior is implemented.

| Name | NID |
| --- | --- |
| `sceFaceTrackerStartTracking` | `bAKsVCOJMu8` |
| `sceFaceTrackerStopTracking` | `Tb-iOd+G2Zs` |
| `sceFaceTrackerReset` | `i0DbE5x06fw` |
| `sceFaceTrackerAddUserFeature` | `XuqmYJfdEzI` |
| `sceFaceTrackerGetWorkingMemorySize` | `zN4908PoI3k` |
| `sceFaceTrackerInitialize` | `SknJO3Bt+-o` |
| `sceFaceTrackerFinalize` | `kgoY4sueNqU` |
| `sceFaceTrackerUpdate` | `ZHWeGLW4X34` |
| `sceFaceTrackerSearchTarget` | `vnp7yntvlPY` |
| `sceFaceTrackerGetResult` | `IV-ddS1D+U0` |

### Implemented

The context layout below was verified against the bundled module disassembly. All implemented functions validate `context` for null (`0x812a0002`) and the initialization flag `context+0x180c` (`0x812a0003`) exactly as the binary does, so they also reproduce the correct error results on an uninitialized or untracked context.

- [The five callback registrations](../../../core/libs/prx/libSceFaceTracker/Export.cpp) - `sceFaceTrackerRegisterStartTrackingCallback` `amMmIL1kj-k`, `sceFaceTrackerRegisterFixUserIdCallback` `glkVxMoEu-E`, `sceFaceTrackerRegisterStopTrackingCallback` `YPqj8Ky3WDY`, `sceFaceTrackerRegisterLostTargetCallback` `knPtjtHAlek`, `sceFaceTrackerRegisterRecoveryFromLostTargetCallback` `85RI6pMiI6c` - store the callback at `context+0x1810`/`+0x1818`/`+0x1820`/`+0x1828`/`+0x1830` and the user argument at `context+0x1838`/`+0x1840`/`+0x1848`/`+0x1850`/`+0x1858`.
- [`sceFaceTrackerRegisterUser`](../../../core/libs/prx/libSceFaceTracker/Export.cpp) `VsVD7+SYlNc` - user database at `context+0x1598`: entry table pointer `+0x28` (0xc-stride records: used byte `+0`, `+4`, userId `+8`), feature blocks pointer `+0x30` (0x4100-stride per user), count `+0x48`. Rejects `userId <= 0`, null feature, and `featureSize` outside `1..0x4100` with `0x812a0001`; a `count <= 0` or fully-used table returns `0x812a0005`; a duplicate userId returns `0x812a0001`. Copies `featureSize` bytes into the first free slot's feature block, marks the entry used, and stores the userId.
- [`sceFaceTrackerUnregisterUser`](../../../core/libs/prx/libSceFaceTracker/Export.cpp) `CAIoDnCFX70` - removes the user entry (clears `+0`/`+4`/`+8`, then re-initializes the 0x4100 feature block: sixteen 0x410 elements zeroed with `0x1ffffffff` written at `+0x400` and `0` at `+0x408`) and clears `userId` fields (`+0x24`) on all track records, exactly like the binary.
- [`sceFaceTrackerGetUserFeature`](../../../core/libs/prx/libSceFaceTracker/Export.cpp) `Ls+C7--QDuo` - on a userId match returns the feature block pointer and `0x4100` through the two out parameters; `0x812a0006` when no user matches. Guest out pointers are safely probed via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` preserving host errno, returning `0x812a0001` on null or unmapped memory as a host-safety fallback.
- [`sceFaceTrackerGetUserIdByTrackId` / `sceFaceTrackerGetTrackIdByUserId`](../../../core/libs/prx/libSceFaceTracker/Export.cpp) `wG6+Pqh1prQ` / `GJQTuIKPCLM` - track records live at `context+0x1780`, count `context+0x1778`, stride `0x1340`; trackId at record `+0x18`, userId at `+0x24`. Negative trackId or non-positive userId returns `0x812a0001`; empty/miss returns `0x812a0006`; on a match the firmware returns the counterpart id as the function result (not through an out pointer).

### Partially decoded, not yet ported

- [`sceFaceTrackerGetDefaultConfig`](../../../core/libs/prx/libSceFaceTracker/Export.cpp) is a five-stage defaults fill, implemented. Sub-fills: `+0x38 = {f32 0.841, f32 2.0}`, `+0x40 = {f32 2.0, u32 2, u32 2, f32 0.3}`, `+0x50 = 4`; `+0x54 = {f32 90.0, u32 5, f32 50.0, u32 10}`, `+0x64 = 1`; `+0x6c = f32 52.0`; inline `+0x68 = 0xf`, `+0x70 = {u32 30, u32 15}`, `+0x78 = 5`, bytes `+0x7c/+0x7d = 1`. The first 0x38 bytes are a field remap of the `0x38`-byte defaults block from `sceFaceDetectionGetDefaultParam` (resolved through `JMPREL[14]` → symtab `ZyC0HzaV6VU`); the bundled module imports it from libSceFace and this port links `libSceFace` for the same call. A null config returns `0x812a0001`; a nonzero return from the face call skips the first block only — firmware discards every sub-fill result and still returns 0. The 0x7e-byte layout is verified up to the `+0x7c` word; whether the caller-visible config struct is exactly 0x80 bytes or larger is unmeasured.
- [`sceFaceTrackerInitialize`](../../../core/libs/prx/libSceFaceTracker/Export.cpp) validates a config struct (`[cfg+0x30] >= 0`, `[cfg+0x70]-1 <= 0x257`, `[cfg+0x74] > 0`, `[cfg+0x78] > 0`, `[cfg+0x6c]` bounded float), queries a two-part working-memory size, rejects re-init of a live context with `0x812a0004`, then builds internal state including a face-analysis engine. Not ported.
- [`sceFaceTrackerStartTracking`/`StopTracking`/`Reset`](../../../core/libs/prx/libSceFaceTracker/Export.cpp) operate on the record table above plus the stop-tracking callback slot and were traced through their internal helpers but not fully reduced; `sceFaceTrackerUpdate` dispatches into the analysis engine (~39 internal calls); `sceFaceTrackerSearchTarget`, `sceFaceTrackerGetResult`, `sceFaceTrackerAddUserFeature`, `sceFaceTrackerGetWorkingMemorySize`, and `sceFaceTrackerFinalize` remain untraced or engine-bound.
