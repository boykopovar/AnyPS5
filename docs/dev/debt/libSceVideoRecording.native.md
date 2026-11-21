# libSceVideoRecording.native technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceVideoRecording.native/Export.cpp) - 27 entries observed in a 3.20-labelled image and a later 13.60-labelled image are import-resolution declarations only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified apart from the checks under Functional; the remaining parameterless `int` declarations are placeholders, not verified ABI contracts. Past those checks every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/752e1b1d4a6ac8a26c97e693527fefbd/a7bddd315c7740a7d065af9e80b0447fcd5b91be) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled module. The extract SHA-256 is `63256453dc9620091ce6af95f8b243b7da8f2ce76a585f4886f76aa6625b5b21`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. The extract is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior. The separately published 3.20 source catalog has a module file but none of these selected NIDs.

| Name | NID |
| --- | --- |
| `sceVideoRecordingCopyBGRAtoP010` | `X1f4SXr5qQY` |
| `sceVideoRecordingDisableScanInEncode` | `byQbu2Mg3DI` |
| `sceVideoRecordingEnableScanInEncode` | `taRss8qdZ9Y` |
| `sceVideoRecordingFinalizeByAppId` | `8edDi-bYdbU` |
| `sceVideoRecordingFinalizeByHandle` | `vQFaqjQmdnw` |
| `sceVideoRecordingGetCountScanInAction` | `qUqHoCwWgjE` |
| `sceVideoRecordingGetFinalizeActiveInfo` | `lplCnUFaGm4` |
| `sceVideoRecordingGetFormatByAppId` | `xNcqZLKU9Wk` |
| `sceVideoRecordingGetFormatExtensionByAppId` | `XJoIh8Asbbk` |
| `sceVideoRecordingGetRecordedDurationByAppId` | `WK59GBCCc6k` |
| `sceVideoRecordingGetRecordedDurationByHandle` | `zepTKsV7mmE` |
| `sceVideoRecordingGetStatusByAppId` | `DA6qMJMGP+U` |
| `sceVideoRecordingGetStatusByHandle` | `ufwOTTG2jV4` |
| `sceVideoRecordingIsProfileChangeSupported` | `AyI9L0Dxm+4` |
| `sceVideoRecordingOpenByRAFeatureId` | `qEfkBZEwu7g` |
| `sceVideoRecordingResetRecordedData` | `oWf+aK9v1Pw` |
| `sceVideoRecordingRestModeProcessStop` | `F8pia9FXpPs` |
| `sceVideoRecordingSetCopyrightImage` | `Augrd4cY6E0` |
| `sceVideoRecordingSetInfoByHandle` | `PmqSOAzftJg` |
| `sceVideoRecordingSetPrivacyImage` | `nzmFH8Adz5c` |
| `sceVideoRecordingSetProfileLimit` | `kEfdObutIDg` |
| `sceVideoRecordingSetResourceStateByHandle` | `BebkD-x+vZc` |
| `sceVideoRecordingStartByHandle` | `ypNPzNhXzRg` |
| `sceVideoRecordingStopByHandle` | `mopHCpOSZT8` |
| `Unknown` | `26XG0o9fx8Q` |
| `Unknown` | `WT4aTfe5OYk` |
| `Unknown` | `iYtbtBx4jvE` |

### Functional

- [sceVideoRecordingFinalizeByAppId, sceVideoRecordingFinalizeByHandle, sceVideoRecordingGetFormatByAppId, sceVideoRecordingGetFormatExtensionByAppId, sceVideoRecordingGetRecordedDurationByAppId and sceVideoRecordingGetStatusByAppId](../../../core/libs/prx/libSceVideoRecording.native/Export.cpp) (libSceVideoRecording.native) - only the argument checks firmware 13.42 makes before it reads any state or makes any call are implemented, from its disassembly, not measured on a console; each returns `0x80a80003`. They are an application ID of -1 (the five ByAppId calls), a null handle (FinalizeByHandle), a second argument of 3 or more (both Finalize calls) and a null buffer or a size below 5 (GetFormatExtensionByAppId). Only the arguments these checks read are declared, and their names are not from a header.
- [sceVideoRecordingSetPrivacyImage, sceVideoRecordingSetProfileLimit and sceVideoRecordingSetResourceStateByHandle](../../../core/libs/prx/libSceVideoRecording.native/Export.cpp) (libSceVideoRecording.native) - only the argument checks firmware 13.42 makes before it reads any state or makes any call are implemented, from its disassembly, not measured on a console; each returns `0x80a80003`. They are a null handle or a state of 2 or more (SetResourceStateByHandle), a limit above 1 (SetProfileLimit) and a third argument above 2 (SetPrivacyImage). Only the arguments these checks read are declared, and their names are not from a header.
- [sceVideoRecordingGetCountScanInAction, sceVideoRecordingGetFinalizeActiveInfo, sceVideoRecordingGetRecordedDurationByHandle, sceVideoRecordingGetStatusByHandle, sceVideoRecordingIsProfileChangeSupported, sceVideoRecordingSetInfoByHandle, sceVideoRecordingStartByHandle and sceVideoRecordingStopByHandle](../../../core/libs/prx/libSceVideoRecording.native/Export.cpp) (libSceVideoRecording.native) - only the argument checks firmware 13.42 makes before it reads any state or makes any call are implemented, from its disassembly, not measured on a console; each returns `0x80a80003`. They are a null handle (the five ByHandle calls here) and a null pointer (the single pointer of GetCountScanInAction, GetFinalizeActiveInfo and IsProfileChangeSupported). Only the arguments these checks read are declared, and their names are not from a header.
