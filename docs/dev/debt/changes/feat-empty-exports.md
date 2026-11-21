# libSceNpWebApi

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceNpWebApi/InventoryExports.cpp) - 36 NIDs occur as functions under the same module and logical library in both version-labelled images. The labels do not independently authenticate retail firmware provenance; the NIDs are absent from the pinned public 3.20 catalogue. Names, prototypes and behavior are unverified. These import-resolution declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality, except `V6DhvHJCGfM` and `eJ1gJsUhQW4`, which are two NIDs for the same function: 13.40 has one byte at 0xe4d0, `ret`, so both answer without reading an argument or touching memory.

# libSceVnaWebsocket

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceVnaWebsocket/Export.cpp) - 18 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry but `Bw0-YBkPNm4` throws through `NotImplemented_nid_no_patch`; that one is a single `ret` at 0x8e0 in 13.40. Unknown names use literal NID aliases. One data export is outside this callable inventory and requires separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

# libSceVdecCore

## change sceVdecCoreAnalyzeStream, sceVdecCoreFinalizeComputeResource

- [sceVdecCoreFinalizeComputeResource, sceVdecCoreInitializeComputeResource, sceVdecCoreSetDecodeOutputSw, sceVdecCoreSyncDecodeOutputSw and sceVdecCoreTrySyncDecodeOutputSw](../../../core/libs/prx/libSceVdecCore/Export.cpp) (libSceVdecCore) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecCore.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified. `sceVdecCoreAnalyzeStream` left this list: 13.40 has `xor eax, eax; ret` (3 bytes at 0x2a0), so it answers 0 without reading an argument.
