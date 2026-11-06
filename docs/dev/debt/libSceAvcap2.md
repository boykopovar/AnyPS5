# libSceAvcap2 technical debt

### Unknown function info

Signatures and behavior are unverified for the 35 additional exports listed below, whose name/NID pairs are recorded in the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceavcap2-9035a2a4ef.jsonl). Parameterless `int` declarations only resolve imports and make no ABI claim; every call throws through `NotImplemented_nid_no_patch`. Not measured on a console.

- [sceAvcap2AllocateVideoEncoderMemory, sceAvcap2AllocateVideoEncoderMemory2, sceAvcap2FreeEncoderVideoMemory, sceAvcap2GetAudioOutVolume, sceAvcap2GetFramePitch, sceAvcap2GetImagePasteMemory, sceAvcap2GetMicVolume](../../../core/libs/prx/libSceAvcap2/Export.cpp) - unknown signatures and behavior; every call throws.
- [sceAvcap2GetNonVclNalUnitStream, sceAvcap2GetNonVclNalUnitStream2, sceAvcap2GetNonVclNalUnitStream3, sceAvcap2GetRecProhibit, sceAvcap2GetSupportInformation, sceAvcap2GetVideoOutMode, sceAvcap2LockInputDataBuffer](../../../core/libs/prx/libSceAvcap2/Export.cpp) - unknown signatures and behavior; every call throws.
- [sceAvcap2MapMemory, sceAvcap2OpenPadSpk, sceAvcap2OpenVideo, sceAvcap2QueryVideoEncoderMemorySize, sceAvcap2ReadPadSpk, sceAvcap2ReadVideo, sceAvcap2ReadVideo2](../../../core/libs/prx/libSceAvcap2/Export.cpp) - unknown signatures and behavior; every call throws.
- [sceAvcap2RequestKeyFrame, sceAvcap2Select, sceAvcap2SetAudioOutVolume, sceAvcap2SetAudioOutVolumeForRec, sceAvcap2SetChapterOpenLevel, sceAvcap2SetChapterPermissionLevel, sceAvcap2SetImagePaste](../../../core/libs/prx/libSceAvcap2/Export.cpp) - unknown signatures and behavior; every call throws.
- [sceAvcap2SetInvalidFrame, sceAvcap2SetMicVolume, sceAvcap2SetMicVolumeForRec, sceAvcap2SetPrivacyGuard, sceAvcap2SetPrivacyImage, sceAvcap2SetVideoEncodeConfig, sceAvcap2UnlockInputDataBuffer](../../../core/libs/prx/libSceAvcap2/Export.cpp) - unknown signatures and behavior; every call throws.

The 7 original exports listed below in [Export.cpp](../../../core/libs/prx/libSceAvcap2/Export.cpp) are declared from the NIDs that the PS5 homebrew project [HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5/blob/main/src/avcap2.c) resolves from `libSceAvcap2` (`avcap2.c`, lines 23-29); each NID was checked to equal the hash of the `sceAvcap2*` name given here. Their prototypes and behaviour are unverified: these functions take no parameters in the import-resolution convention and throw through `NotImplemented_nid_no_patch`.

- [sceAvcap2Initialize](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID svzPXluOz8U)
- [sceAvcap2Terminate](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID gojqghDU+1Y)
- [sceAvcap2OpenAudio](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID 3nHpE7Dp5SE)
- [sceAvcap2Close](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID tR7gPe1i8hw)
- [sceAvcap2Start](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID 8CGNCwBsItI)
- [sceAvcap2Stop](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID Z5dKK0xnQ8g)
- [sceAvcap2ReadAudio](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID WhsHggqdwcg)

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAvcap2/Export.cpp) - 12 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled image. The extract SHA-256 is `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. This is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior.

| Name | NID |
| --- | --- |
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
