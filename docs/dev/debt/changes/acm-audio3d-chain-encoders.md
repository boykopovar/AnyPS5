# libSceAcm

## remove `sceAcmChain_Audio3dPO1_Clear`

## remove `sceAcmChain_Audio3dPO1_Activate`

## remove `sceAcmChain_Audio3dPO1_Output`

## remove `sceAcmChain_Audio3dPO1_RenderAmbisonicsChannel`

## remove `sceAcmChain_Audio3dPO2_Activate`

## remove `sceAcmChain_Audio3dPO1_RenderPassthroughObject`

## remove `sceAcmChain_Audio3dPO2_RenderDelay`

## change [Additional ACM exports]
- [Additional ACM exports](../../../core/libs/prx/libSceAcm/Export.cpp) - the four name/NID pairs below are defined exports in the [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceacm-a3566cc281.jsonl). Their ABIs and behaviour are unknown. Parameterless `int` declarations are import-resolution placeholders, not established signatures; every call throws `NotImplemented_nid_no_patch`. No telemetry, shared-IR or PO1 object-render behaviour is implemented.

## add Functional
- [Audio3dPO1/PO2 chain encoders](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - `sceAcmChain_Audio3dPO1_Clear`, `sceAcmChain_Audio3dPO1_Activate`, `sceAcmChain_Audio3dPO1_Output`, `sceAcmChain_Audio3dPO1_RenderAmbisonicsChannel`, `sceAcmChain_Audio3dPO2_Activate`, `sceAcmChain_Audio3dPO1_RenderPassthroughObject` and `sceAcmChain_Audio3dPO2_RenderDelay` access caller structs through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`, preserving `errno` and returning `0x81940006` (`SCE_ACM_ERROR_BAD_POINTER`) on unmapped or unwritable pointers instead of faulting.

## add Unknown function info
- [Audio3dPO1/PO2 chain encoders](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - the 7 implemented `sceAcmChain_Audio3dPO1_*` and `sceAcmChain_Audio3dPO2_*` batch encoders follow `libSceAcm.sprx` disassembly (not measured on a console); full chain and state struct layouts beyond the accessed offsets and the DSP interpretation of the encoded command words and delay descriptors remain unverified.
