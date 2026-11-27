# libSceEsvmEngine technical debt

### Unknown function info

- [Additional libSceEsvmEngine exports](../../../core/libs/prx/libSceEsvmEngine/Export.cpp) - the 5 name/NID pairs below are defined function exports in the pinned [13.60-labelled inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceesvmengine-c1d3867cc5.jsonl). Their signatures and behavior are unknown; parameterless `int` declarations are import placeholders, not verified ABI prototypes. Every call throws through `NotImplemented_nid_no_patch`. No media processing, decryption or output behavior is implemented. The inventory label is not an independent console measurement.

| Name | NID |
| --- | --- |
| `sceEsvmEngineGetMetrics` | `gh3Zne9NOHc` |
| `sceEsvmEngineMediaKeySessionDecryptAudioData` | `tRvIFde5wIY` |
| `sceEsvmEngineMediaKeysDecryptAudioData` | `bJlgPqkTyHI` |
| `sceEsvmEngineMediaKeysSetAppParameter` | `oTDWoz5VJ4c` |
| `sceEsvmEngineProcessAckResponse` | `RIWSlTAAgO4` |

- [sceEsvmEngineDestroyMediaKeySystemAccess, sceEsvmEngineMediaKeySessionClose, sceEsvmEngineMediaKeySessionGenerateRequest, sceEsvmEngineMediaKeySessionGetExpiration, sceEsvmEngineMediaKeySessionGetKeyStatuses, sceEsvmEngineMediaKeySessionGetSessionId, sceEsvmEngineMediaKeySessionLoad, sceEsvmEngineMediaKeySessionRemove, sceEsvmEngineMediaKeySessionSetEventHandler, sceEsvmEngineMediaKeySessionUpdate, sceEsvmEngineMediaKeySystemAccessCreateMediaKeys, sceEsvmEngineMediaKeySystemAccessDestroyMediaKeys, sceEsvmEngineMediaKeySystemAccessGetConfiguration, sceEsvmEngineMediaKeySystemAccessSetCdmStorageConfig, sceEsvmEngineMediaKeysCreateSession, sceEsvmEngineMediaKeysDestroySession, sceEsvmEngineMediaKeysSetEventHandler, sceEsvmEngineMediaKeysSetServerCertificate and sceEsvmEngineRequestMediaKeySystemAccess](../../../core/libs/prx/libSceEsvmEngine/Export.cpp) (libSceEsvmEngine) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceEsvmEngine.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

- [The uninitialized answers](../../../core/libs/prx/libSceEsvmEngine/Export.cpp) (libSceEsvmEngine) - 21 context, decoder, playback, frame and media-key-binding exports (NID `TLCfQqZ-6AU` among them) return `0x8178000A`, only the 32-bit result being claimed, because `sceEsvmEngineInitialize` throws and the library is never initialized here. In firmware 13.42's `libSceEsvmEngine.sprx` (SHA-256 `da39a4d6ec49cb45bf2b15e742d7a7182360c0d69458201b5af91afe5b47d2e4`), read from disassembly and not run on a console, each tests the byte at `0x3896d8` before any argument is read or any call is made and returns `0x8178000A` unless it is 1. That byte lies beyond its segment's file data, so it starts zero, and the only store to it in the executable segment is `movb $0x1` at `0x264c9` in `sceEsvmEngineInitialize`. Names, signatures and the error's name are unverified.

- [sceEsvmEngineInitializeCdm, sceEsvmEngineQueryVideoMemory and `Y6Rtwfuj0Bw`](../../../core/libs/prx/libSceEsvmEngine/Export.cpp) (libSceEsvmEngine) - only the null check of the first argument is modelled: each returns `0x81780001` for a null pointer and throws otherwise; what they do then, the pointer's layout and any further arguments are unknown, and the argument type is inferred from register use. Firmware 13.42's `libSceEsvmEngine.sprx` (SHA-256 `da39a4d6ec49cb45bf2b15e742d7a7182360c0d69458201b5af91afe5b47d2e4`), read from disassembly and run under a CPU emulator on x86-64, not on a console, tests the first argument before any call in each of them.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceEsvmEngine/Export.cpp) - 10 entries observed in a 3.20-labelled image and a later 13.60-labelled image are import-resolution declarations only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/752e1b1d4a6ac8a26c97e693527fefbd/a7bddd315c7740a7d065af9e80b0447fcd5b91be) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled module. The extract SHA-256 is `63256453dc9620091ce6af95f8b243b7da8f2ce76a585f4886f76aa6625b5b21`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. The extract is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior. The separately published 3.20 source catalog has a module file but none of these selected NIDs.

| Name | NID |
| --- | --- |
| `sceEsvmEngineGetProvisioningRequest` | `9biIbMBK5Ko` |
| `sceEsvmEngineGetServiceCertificateRequest` | `qp4iYAwTVOs` |
| `sceEsvmEngineHandleProvisioningResponse` | `Ec5R6hlGOi0` |
| `sceEsvmEngineInitialize` | `fo4jUOJKgLQ` |
| `sceEsvmEngineIsProvisioned` | `O-viosYkkcA` |
| `sceEsvmEngineMediaKeySystemAccessCreateMediaKeys2` | `-14JqmMs00Y` |
| `sceEsvmEngineMediaKeySystemAccessGetKeySystem` | `r1PRJWghAn0` |
| `sceEsvmEngineParseAndLoadServerCertificateResponse` | `1MtR379QDeI` |
| `sceEsvmEngineSetServerCertificate2` | `bdutxKejMx8` |
| `sceEsvmEngineTerminate` | `e2j1lH088qw` |
