# libSceEsvm technical debt

### Unknown function info

- [Additional libSceEsvm exports](../../../core/libs/prx/libSceEsvm/Export.cpp) - the 45 name/NID pairs below are defined function exports in the pinned [13.60-labelled inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceesvm-84c73245ba.jsonl). Their signatures and behavior are unknown; parameterless `int` declarations are import placeholders, not verified ABI prototypes. Every call throws through `NotImplemented_nid_no_patch`. No media processing, decryption or output behavior is implemented. The inventory label is not an independent console measurement.

| Name | NID |
| --- | --- |
| `sceEsvmAddDecoder` | `2ALGsNZHUQY` |
| `sceEsvmAddDecoder2` | `ZqZiBfUv89c` |
| `sceEsvmCheckCallback` | `U9B-hD7Sqb4` |
| `sceEsvmCreateContext` | `047hNdtrcw4` |
| `sceEsvmCreateMediaKeySession` | `0I6PyLF6VGw` |
| `sceEsvmCreateMediaKeys` | `tTiBOviUHz4` |
| `sceEsvmDestroyContext` | `wOdoqv-EiKU` |
| `sceEsvmDestroyMediaKeySession` | `iCEcrEPsshU` |
| `sceEsvmDestroyMediaKeySystemAccess` | `pMHQM54XuSs` |
| `sceEsvmDestroyMediaKeys` | `Qww8wL0TmCk` |
| `sceEsvmFlush` | `w2N939ABS2Y` |
| `sceEsvmInitialize` | `h59QfNm3QKU` |
| `sceEsvmMediaKeySessionClose` | `TwlVpcBFeBM` |
| `sceEsvmMediaKeySessionDecryptAudioData` | `5Mq8RIwHCQ4` |
| `sceEsvmMediaKeySessionGenerateRequest` | `CD4uF8+rvIA` |
| `sceEsvmMediaKeySessionGetExpiration` | `Ket33imTVLw` |
| `sceEsvmMediaKeySessionGetKeyStatuses` | `aswwrgJ7I7k` |
| `sceEsvmMediaKeySessionGetSessionId` | `GKKv1LQWTIE` |
| `sceEsvmMediaKeySessionLoad` | `eSsrvlpgLtQ` |
| `sceEsvmMediaKeySessionRemove` | `LtFvG9euwwo` |
| `sceEsvmMediaKeySessionUpdate` | `QaDDoPYv9BY` |
| `sceEsvmMute` | `X33X6RmLLX0` |
| `sceEsvmPause` | `+iomwyWJU4o` |
| `sceEsvmPlay` | `fIdN5WzxNXg` |
| `sceEsvmPushAccessUnit` | `1XFXci4p4uU` |
| `sceEsvmReleaseVideoFrame` | `nz-tYwNrKAQ` |
| `sceEsvmRender` | `7h7bYwQQoOg` |
| `sceEsvmRenderById` | `dYfk+OPPpZU` |
| `sceEsvmSetAudiblePlaybackRate` | `A-fwc6NXEGA` |
| `sceEsvmSetCodecConfig` | `hG0sePjh+jk` |
| `sceEsvmSetPlaybackRate` | `DzqQnbjNcUo` |
| `sceEsvmSetVolume` | `Vpcm2hauAJs` |
| `sceEsvmTerminate` | `39VZ-wS-7mI` |
| `sceEsvmVideoOutAllowOutputResolutionWqhdDetection` | `yhZXWGA5+bM` |
| `sceEsvmVideoOutConfigureOutputEx` | `5pEox4L+M70` |
| `sceEsvmVideoOutGetMonitorInfo` | `UbQ5Ly--4bY` |
| `sceEsvmVideoOutGetOutputStatus` | `7iomFnqJKm8` |
| `sceEsvmVideoOutInitializeOutputOptions` | `GeTQwXMknSg` |
| `sceEsvmVideoOutIsHdcp14Enabled` | `GB2M1HPufrw` |
| `sceEsvmVideoOutIsHdcp23Enabled` | `LLr7I+Z8JgM` |
| `sceEsvmVideoOutIsOutputSupportedByMonitorInfo` | `7eWKfQClQ7M` |
| `sceEsvmVideoOutIsPortDeviceConnected` | `vD3nEWBfhLY` |
| `sceEsvmVideoOutIsPortStatusUsable` | `ouGTOwK1yDc` |
| `sceEsvmVideoOutSetContentDynamicRange` | `UEd8lhAwqXw` |
| `sceEsvmVideoOutSetContentType` | `oUGLWJNfVwM` |

- [sceEsvmAppAudioSyncCreateContext, sceEsvmAppAudioSyncFlush and sceEsvmSyncVideo](../../../core/libs/prx/libSceEsvm/Export.cpp) (libSceEsvm) - signatures and behavior are unverified; these parameterless declarations only resolve imports and throw through `NotImplemented_nid_no_patch`.
