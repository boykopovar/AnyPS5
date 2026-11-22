# libSceVideoStreamingEngine technical debt

### Unknown function info

- [Additional libSceVideoStreamingEngine exports](../../../core/libs/prx/libSceVideoStreamingEngine/Export.cpp) - the 12 name/NID pairs below are defined function exports in the pinned [13.60-labelled inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscevideostreamingengine-5abc5612ed.jsonl). Their signatures and behavior are unknown; parameterless `int` declarations are import placeholders, not verified ABI prototypes. Every call throws through `NotImplemented_nid_no_patch`. No media processing, decryption or output behavior is implemented. The inventory label is not an independent console measurement.

| Name | NID |
| --- | --- |
| `sceVideoStreamingEngineInitialize` | `kruTFfx-BDM` |
| `sceVideoStreamingEngineIsTransparentVideoMode` | `vLAERdvUknc` |
| `sceVideoStreamingEngineMediaKeySessionClose` | `RWCz4Gc+lW0` |
| `sceVideoStreamingEngineMediaKeySessionRemove` | `MB+kfLoZFWE` |
| `sceVideoStreamingEngineMediaKeySessionUpdate` | `+gqhFOBhLj4` |
| `sceVideoStreamingEngineMediaKeySystemAccessGetConfiguration` | `l42jJq+1X9M` |
| `sceVideoStreamingEngineMediaKeysDestroySession` | `tUuzW-ctqRY` |
| `sceVideoStreamingEngineMediaKeysSetServerCertificate` | `o9B+2IRDKow` |
| `sceVideoStreamingEngineQueryInitEsvmMemory` | `ptFhKGXird8` |
| `sceVideoStreamingEngineQueryVideoMemory` | `x32O4iKHZyA` |
| `sceVideoStreamingEngineRequestMediaKeySystemAccess` | `Y-Bp-73pyIs` |
| `sceVideoStreamingEngineTerminate` | `bbKGMzUU5fA` |

- [The uninitialized answers](../../../core/libs/prx/libSceVideoStreamingEngine/Export.cpp) (libSceVideoStreamingEngine) - 46 player, source buffer, track, sample, thumbnail and auxiliary message exports return `0x8178000A`, only the 32-bit result being claimed, because `sceVideoStreamingEngineInitialize` throws and the library is never initialized here. In firmware 13.42's `libSceVideoStreamingEngine.sprx` (SHA-256 `5572e250944e5f37c7fcc050db71ce36d110e4294889b4ab6c44d5e8e1de65ba`), read from disassembly and not run on a console, each of them tests the byte at `0x499958` before any argument is read or any call is made and returns `0x8178000A` unless it is 1. That byte lies beyond its segment's file data, so it starts zero, and the only store to it in the executable segment is `movb $0x1` at `0x49299` in `sceVideoStreamingEngineInitialize`. Names, signatures and the error's name are unverified.

- [sceVideoStreamingEngineDestroyMediaKeySystemAccess, sceVideoStreamingEngineMediaKeySessionGenerateRequest, sceVideoStreamingEngineMediaKeySessionGetExpiration, sceVideoStreamingEngineMediaKeySessionGetKeyStatuses, sceVideoStreamingEngineMediaKeySessionGetSessionId, sceVideoStreamingEngineMediaKeySessionLoad, sceVideoStreamingEngineMediaKeySessionSetEventHandler, sceVideoStreamingEngineMediaKeySystemAccessCreateMediaKeys, sceVideoStreamingEngineMediaKeySystemAccessDestroyMediaKeys, sceVideoStreamingEngineMediaKeySystemAccessGetKeySystem, sceVideoStreamingEngineMediaKeySystemAccessSetCdmStorageConfig and sceVideoStreamingEngineMediaKeysCreateSession](../../../core/libs/prx/libSceVideoStreamingEngine/Export.cpp) (libSceVideoStreamingEngine) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVideoStreamingEngine.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
