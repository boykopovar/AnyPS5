# libSceLoginMgrServer technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceLoginMgrServer/Export.cpp) - 5 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry answers only as the module does before initialization (see Functional) and is otherwise not implemented. Unidentified names use literal NID aliases. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `GzbYqYyRTBg` |
| `Unknown` | `M1h7vFE7Hm0` |
| `Unknown` | `NvO5e-NOkXU` |
| `Unknown` | `T7HQTX-ylss` |
| `Unknown` | `uD-Sde+R4D8` |

### Unknown function info

- [sceLoginMgrServerInitialize and sceLoginMgrServerIsRequestedCdlgClose](../../../core/libs/prx/libSceLoginMgrServer/Export.cpp) (libSceLoginMgrServer) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLoginMgrServer.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [sceLoginMgrServerNotifySafetyNoticeShownForGen2Vr, sceLoginMgrServerCheckSafetyNoticeShownForGen2Vr](../../../core/libs/prx/libSceLoginMgrServer/Export.cpp) have unknown signatures and behaviour beyond the answer before initialization (see Functional). The parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures. Only the names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceloginmgrserver-9d8badaba5.jsonl). These are system/VR UI-facing inventory entries only; safety-notice handling, VR behaviour and title use remain unverified.

### Functional

- [The answers before initialization](../../../core/libs/prx/libSceLoginMgrServer/Export.cpp) (libSceLoginMgrServer) - `sceLoginMgrServerInitialize` throws, so the library is never initialized here, and the other exports apart from `sceLoginMgrServerIsRequestedCdlgClose` return only `0x813E0001`; only the 32-bit result is claimed. In firmware 13.42's `libSceLoginMgrServer.sprx` (SHA-256 `0312dff8c52048ddea216c15af57f90b4c081824d475f7e07aa1ba71449ec6f4`), read from disassembly and not run on a console, each export passes the object at `0x10018` to a method that locks its mutex and returns `0x813E0001` after unlocking while the count at `0x10028` is 0, before reading any argument. The constructor `0x1c40`, run at load from the list at `0xc0e0`, stores 0 there; only a successful initialize (`0x1dfe`) raises it, and terminate lowers it. `sceLoginMgrServerIsRequestedCdlgClose` (`0x25b0`) has no such check.
