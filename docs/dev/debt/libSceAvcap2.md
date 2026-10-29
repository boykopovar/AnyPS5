# libSceAvcap2 technical debt

### Unknown function info

The 7 exports in [Export.cpp](../../../core/libs/prx/libSceAvcap2/Export.cpp) are declared from the NIDs that the PS5 homebrew project [HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5/blob/main/src/avcap2.c) resolves from `libSceAvcap2` (`avcap2.c`, lines 23-29); each NID was checked to equal the hash of the `sceAvcap2*` name given here. Whether these are all the exports of the PS5 module is unknown, and prototypes and behaviour are unverified: every function takes no parameters in the import-resolution convention and throws through `NotImplemented_nid_no_patch`.

- [sceAvcap2Initialize](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID svzPXluOz8U)
- [sceAvcap2Terminate](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID gojqghDU+1Y)
- [sceAvcap2OpenAudio](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID 3nHpE7Dp5SE)
- [sceAvcap2Close](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID tR7gPe1i8hw)
- [sceAvcap2Start](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID 8CGNCwBsItI)
- [sceAvcap2Stop](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID Z5dKK0xnQ8g)
- [sceAvcap2ReadAudio](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID WhsHggqdwcg)

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAvcap2/Export.cpp) - 41 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled image. The extract SHA-256 is `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. This is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior.

| Name | NID |
| --- | --- |
| `sceAvcap2AllocateVideoEncoderMemory` | `CYgd06bO5q0` |
| `sceAvcap2FreeEncoderVideoMemory` | `96UQhNnBlxc` |
| `sceAvcap2GetAudioOutVolume` | `mYgFfxPwEjY` |
| `sceAvcap2GetFramePitch` | `2GC2UnBJHIA` |
| `sceAvcap2GetMicVolume` | `41+5ixaiTj0` |
| `sceAvcap2GetNonVclNalUnitStream2` | `VAFgNc0Pfp8` |
| `sceAvcap2GetNonVclNalUnitStream` | `NeKlrUv+lbY` |
| `sceAvcap2GetRecProhibit` | `pEm2jVo44dE` |
| `sceAvcap2GetSupportInformation` | `RW88AiwmL+Y` |
| `sceAvcap2GetVideoOutMode` | `DHFDHdKAr9U` |
| `sceAvcap2LockInputDataBuffer` | `S4nvYDibKBw` |
| `sceAvcap2MapMemory` | `X8c-f8LgzHo` |
| `sceAvcap2OpenVideo` | `4jwUgirLFrA` |
| `sceAvcap2QueryVideoEncoderMemorySize` | `Dht3tcbRoEY` |
| `sceAvcap2ReadVideo` | `pN0hIQZsMzc` |
| `sceAvcap2RequestKeyFrame` | `YaXDckFygvw` |
| `sceAvcap2Select` | `ysFbdZrSCMM` |
| `sceAvcap2SetAudioOutVolume` | `4S4mGtzLmyI` |
| `sceAvcap2SetAudioOutVolumeForRec` | `0W6GB-ecnK0` |
| `sceAvcap2SetChapterOpenLevel` | `r9BQzY0IvxA` |
| `sceAvcap2SetChapterPermissionLevel` | `GBx9f5BmDvU` |
| `sceAvcap2SetImagePaste` | `Cdf6VsJgvaY` |
| `sceAvcap2SetInvalidFrame` | `fLzv9I4mHF0` |
| `sceAvcap2SetMicVolume` | `BPgql5PsMKo` |
| `sceAvcap2SetMicVolumeForRec` | `2Lv-4HVkqO4` |
| `sceAvcap2SetPrivacyGuard` | `aAbMwQwWHyM` |
| `sceAvcap2SetPrivacyImage` | `ImOtOnPjgio` |
| `sceAvcap2SetVideoEncodeConfig` | `-NotZDC2pQc` |
| `sceAvcap2UnlockInputDataBuffer` | `BjHu5q5-6NI` |
| `Unknown` | `+OspdnZ-8mc` |
| `Unknown` | `1Xp85wUaVTE` |
| `Unknown` | `32gujAUoIMc` |
| `Unknown` | `4hkuDvkGu7U` |
| `Unknown` | `7KBaem5GyBQ` |
| `Unknown` | `88YEF+XHfRY` |
| `Unknown` | `HAeczEM-m7c` |
| `Unknown` | `HrGLkn9o4MY` |
| `Unknown` | `Tzk7-W5mp0A` |
| `Unknown` | `VNRuuZQx7gE` |
| `Unknown` | `iy6ZcsdeoMk` |
| `Unknown` | `k7gz608olKM` |
