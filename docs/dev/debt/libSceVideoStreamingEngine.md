# libSceVideoStreamingEngine technical debt

### Unknown function info

- [Additional libSceVideoStreamingEngine exports](../../../core/libs/prx/libSceVideoStreamingEngine/Export.cpp) - the 58 name/NID pairs below are defined function exports in the pinned [13.60-labelled inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscevideostreamingengine-5abc5612ed.jsonl). Their signatures and behavior are unknown; parameterless `int` declarations are import placeholders, not verified ABI prototypes. Every call throws through `NotImplemented_nid_no_patch`. No media processing, decryption or output behavior is implemented. The inventory label is not an independent console measurement.

| Name | NID |
| --- | --- |
| `sceVideoStreamingEngineAbortAuxMessage` | `G7mvPDnYwQQ` |
| `sceVideoStreamingEngineAbortParsing` | `fb3kqOU6eiI` |
| `sceVideoStreamingEngineAddSourceBuffer` | `ncXmalKo5TE` |
| `sceVideoStreamingEngineAppendBuffer` | `s3oaAI0fKwU` |
| `sceVideoStreamingEngineCancelThumbnail` | `ob+vzpK-ePI` |
| `sceVideoStreamingEngineClearLiveSeekableRange` | `YZnFDSSQFE8` |
| `sceVideoStreamingEngineCreatePlayer` | `VY4Mr66uSqA` |
| `sceVideoStreamingEngineDeleteMediaSample` | `SfDRCMJoaLE` |
| `sceVideoStreamingEngineDeletePlayer` | `TBPjAg6a2E4` |
| `sceVideoStreamingEngineDivideMediaSample` | `hZEb-EhU-4g` |
| `sceVideoStreamingEngineEnableAudioTrack` | `GFPBvjLfimA` |
| `sceVideoStreamingEngineEndOfStream` | `j4b+RLwmg3k` |
| `sceVideoStreamingEngineEnqueueMediaSample` | `G9GFBN1xtH8` |
| `sceVideoStreamingEngineGenerateThumbnail` | `rcTQ2SilW60` |
| `sceVideoStreamingEngineGetAudioFrame` | `Svch6mxCneE` |
| `sceVideoStreamingEngineGetAudioTrackInfo` | `6x9vvo92flo` |
| `sceVideoStreamingEngineGetBufferedInfo` | `Q2+ul4Jj7-4` |
| `sceVideoStreamingEngineGetDuration` | `tFgCMXYLEYo` |
| `sceVideoStreamingEngineGetLoadingProgress` | `w6Y73Ubl0jw` |
| `sceVideoStreamingEngineGetLoop` | `x7Vr+deBKo0` |
| `sceVideoStreamingEngineGetMediaKeys` | `EhFuWvHld1w` |
| `sceVideoStreamingEngineGetPlaybackRange` | `Xq9X3kt49Fk` |
| `sceVideoStreamingEngineGetPlaybackRate` | `-RBoTh3JsaM` |
| `sceVideoStreamingEngineGetResolution` | `f8LzETNbCYk` |
| `sceVideoStreamingEngineGetTimeProperty` | `p1jJjKjTEvU` |
| `sceVideoStreamingEngineGetVideoTrackInfo` | `5-P4R09btQQ` |
| `sceVideoStreamingEngineInitialize` | `kruTFfx-BDM` |
| `sceVideoStreamingEngineIsLiveStreaming` | `tcZ2XNW8PO4` |
| `sceVideoStreamingEngineIsTransparentVideoMode` | `vLAERdvUknc` |
| `sceVideoStreamingEngineLoad` | `iv5nnUPe-ls` |
| `sceVideoStreamingEngineMediaKeySessionClose` | `RWCz4Gc+lW0` |
| `sceVideoStreamingEngineMediaKeySessionRemove` | `MB+kfLoZFWE` |
| `sceVideoStreamingEngineMediaKeySessionUpdate` | `+gqhFOBhLj4` |
| `sceVideoStreamingEngineMediaKeySystemAccessGetConfiguration` | `l42jJq+1X9M` |
| `sceVideoStreamingEngineMediaKeysDestroySession` | `tUuzW-ctqRY` |
| `sceVideoStreamingEngineMediaKeysSetServerCertificate` | `o9B+2IRDKow` |
| `sceVideoStreamingEnginePause` | `WFsVlc9eIQs` |
| `sceVideoStreamingEnginePlay` | `VInB8YRmmZg` |
| `sceVideoStreamingEngineQueryInitEsvmMemory` | `ptFhKGXird8` |
| `sceVideoStreamingEngineQueryVideoMemory` | `x32O4iKHZyA` |
| `sceVideoStreamingEngineReleaseThumbnailData` | `bg-209imcPc` |
| `sceVideoStreamingEngineRemoveSourceBuffer` | `zNMGLpbvKpw` |
| `sceVideoStreamingEngineRequestMediaKeySystemAccess` | `Y-Bp-73pyIs` |
| `sceVideoStreamingEngineSeek` | `EYLkNGuJu+k` |
| `sceVideoStreamingEngineSelectVideoTrack` | `nEAyXrcgaMY` |
| `sceVideoStreamingEngineSendAuxMessageForRepeatedResults` | `u3lPXPjdo2k` |
| `sceVideoStreamingEngineSendAuxMessageSync` | `7i7Em8QPYIU` |
| `sceVideoStreamingEngineSetDuration` | `KjTwhkkB52s` |
| `sceVideoStreamingEngineSetLiveSeekableRange` | `UWEHx8qeF6Y` |
| `sceVideoStreamingEngineSetLoop` | `dC6mRLm1HQc` |
| `sceVideoStreamingEngineSetMediaKeys` | `sDX++cFuQiE` |
| `sceVideoStreamingEngineSetPlaybackRange` | `q04I0wREB5c` |
| `sceVideoStreamingEngineSetPlaybackRate` | `eXS-hegAVDU` |
| `sceVideoStreamingEngineSetPtsDtsToMediaSample` | `d3AkZn2emRY` |
| `sceVideoStreamingEngineStop` | `IX7AgdtkNsI` |
| `sceVideoStreamingEngineStopDecode` | `-SNGJumdRkc` |
| `sceVideoStreamingEngineTerminate` | `bbKGMzUU5fA` |
| `sceVideoStreamingEngineUpdateByteStreamFormatSpecification` | `Qg0tKcaHdCA` |

- [sceVideoStreamingEngineDestroyMediaKeySystemAccess, sceVideoStreamingEngineMediaKeySessionGenerateRequest, sceVideoStreamingEngineMediaKeySessionGetExpiration, sceVideoStreamingEngineMediaKeySessionGetKeyStatuses, sceVideoStreamingEngineMediaKeySessionGetSessionId, sceVideoStreamingEngineMediaKeySessionLoad, sceVideoStreamingEngineMediaKeySessionSetEventHandler, sceVideoStreamingEngineMediaKeySystemAccessCreateMediaKeys, sceVideoStreamingEngineMediaKeySystemAccessDestroyMediaKeys, sceVideoStreamingEngineMediaKeySystemAccessGetKeySystem, sceVideoStreamingEngineMediaKeySystemAccessSetCdmStorageConfig and sceVideoStreamingEngineMediaKeysCreateSession](../../../core/libs/prx/libSceVideoStreamingEngine/Export.cpp) (libSceVideoStreamingEngine) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVideoStreamingEngine.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
