# libSceEsvm technical debt

### Unknown function info

- [sceEsvmAppAudioSyncCreateContext, sceEsvmAppAudioSyncFlush and sceEsvmSyncVideo](../../../core/libs/prx/libSceEsvm/Export.cpp) (libSceEsvm) - signatures and behavior are unverified; these parameterless declarations only resolve imports and throw through `NotImplemented_nid_no_patch`.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceEsvm/Export.cpp) - 36 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled image. The extract SHA-256 is `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. This is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior.

| Name | NID |
| --- | --- |
| `sceEsvmAddDecoder` | `2ALGsNZHUQY` |
| `sceEsvmCheckCallback` | `U9B-hD7Sqb4` |
| `sceEsvmCreateContext` | `047hNdtrcw4` |
| `sceEsvmCreateMediaKeySession` | `0I6PyLF6VGw` |
| `sceEsvmCreateMediaKeys` | `tTiBOviUHz4` |
| `sceEsvmDestroyContext` | `wOdoqv-EiKU` |
| `sceEsvmDestroyMediaKeySession` | `iCEcrEPsshU` |
| `sceEsvmDestroyMediaKeySystemAccess` | `pMHQM54XuSs` |
| `sceEsvmDestroyMediaKeys` | `Qww8wL0TmCk` |
| `sceEsvmFlush` | `w2N939ABS2Y` |
| `sceEsvmInitializePlayReady` | `9FyuJdx5q9s` |
| `sceEsvmInitializeWidevine` | `9+Iejj-syHc` |
| `sceEsvmInitialize` | `h59QfNm3QKU` |
| `sceEsvmMediaKeySessionClose` | `TwlVpcBFeBM` |
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
| `sceEsvmRequestMediaKeySystemAccessPlayReady` | `UOTAc1GJCLI` |
| `sceEsvmRequestMediaKeySystemAccessWidevine` | `bYyMqndqumU` |
| `sceEsvmSetCodecConfig` | `hG0sePjh+jk` |
| `sceEsvmSetPlaybackRate` | `DzqQnbjNcUo` |
| `sceEsvmSetVolume` | `Vpcm2hauAJs` |
| `sceEsvmTerminate` | `39VZ-wS-7mI` |
| `sceEsvmVideoOutGetOutputStatus` | `7iomFnqJKm8` |
| `sceEsvmVideoOutIsHdcp14Enabled` | `GB2M1HPufrw` |
| `sceEsvmVideoOutIsHdcp23Enabled` | `LLr7I+Z8JgM` |
| `sceEsvmVideoOutIsPortDeviceConnected` | `vD3nEWBfhLY` |
| `sceEsvmVideoOutIsPortStatusUsable` | `ouGTOwK1yDc` |
