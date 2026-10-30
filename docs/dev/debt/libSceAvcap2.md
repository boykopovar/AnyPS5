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
