# libSceEsvmEngine technical debt

### Unknown function info

- [Additional libSceEsvmEngine exports](../../../core/libs/prx/libSceEsvmEngine/Export.cpp) - the 13 name/NID pairs below are defined function exports in the pinned [13.60-labelled inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceesvmengine-c1d3867cc5.jsonl). Their signatures and behavior are unknown; parameterless `int` declarations are import placeholders, not verified ABI prototypes. Every call throws through `NotImplemented_nid_no_patch`. No media processing, decryption or output behavior is implemented. The inventory label is not an independent console measurement.

| Name | NID |
| --- | --- |
| `sceEsvmEngineAddDecoder2` | `Ta5fE19XPak` |
| `sceEsvmEngineAppAudioSyncFlush` | `X-FCXoSphI8` |
| `sceEsvmEngineGetMetrics` | `gh3Zne9NOHc` |
| `sceEsvmEngineGetVideoPlaybackQuality` | `N8XIjTVfG0I` |
| `sceEsvmEngineInitializeCdm` | `RoPRqUDZyZ8` |
| `sceEsvmEngineMediaKeySessionDecryptAudioData` | `tRvIFde5wIY` |
| `sceEsvmEngineMediaKeysDecryptAudioData` | `bJlgPqkTyHI` |
| `sceEsvmEngineMediaKeysSetAppParameter` | `oTDWoz5VJ4c` |
| `sceEsvmEngineProcessAckResponse` | `RIWSlTAAgO4` |
| `sceEsvmEngineReleaseVideoFrame` | `hrURCROOha8` |
| `sceEsvmEngineSetAudiblePlaybackRate` | `SVxIJmOxpaA` |
| `sceEsvmEngineSetMediaKeys` | `KkzmX+PA+Hg` |
| `sceEsvmEngineSyncVideo` | `ZybN1iURGkk` |

- [sceEsvmEngineDestroyMediaKeySystemAccess, sceEsvmEngineMediaKeySessionClose, sceEsvmEngineMediaKeySessionGenerateRequest, sceEsvmEngineMediaKeySessionGetExpiration, sceEsvmEngineMediaKeySessionGetKeyStatuses, sceEsvmEngineMediaKeySessionGetSessionId, sceEsvmEngineMediaKeySessionLoad, sceEsvmEngineMediaKeySessionRemove, sceEsvmEngineMediaKeySessionSetEventHandler, sceEsvmEngineMediaKeySessionUpdate, sceEsvmEngineMediaKeySystemAccessCreateMediaKeys, sceEsvmEngineMediaKeySystemAccessDestroyMediaKeys, sceEsvmEngineMediaKeySystemAccessGetConfiguration, sceEsvmEngineMediaKeySystemAccessSetCdmStorageConfig, sceEsvmEngineMediaKeysCreateSession, sceEsvmEngineMediaKeysDestroySession, sceEsvmEngineMediaKeysSetEventHandler, sceEsvmEngineMediaKeysSetServerCertificate and sceEsvmEngineRequestMediaKeySystemAccess](../../../core/libs/prx/libSceEsvmEngine/Export.cpp) (libSceEsvmEngine) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceEsvmEngine.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
