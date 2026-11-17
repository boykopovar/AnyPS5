# libSceNpTcs technical debt

### Unknown function info

- [sceNpTcsIntDownloadData, sceNpTcsIntUploadDataToNewSlotAbort and sceNpTcsIntUploadDataToNewSlotBegin](../../../core/libs/prx/libSceNpTcs/Export.cpp) (libSceNpTcs) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpTcs.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

- [Older callable NIDs](../../../core/libs/prx/libSceNpTcs/Export.cpp): The [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) names these exports `sceNpTcsIntAbortRequest`, `sceNpTcsIntCreateContext`, `sceNpTcsIntCreateRequest`, `sceNpTcsIntDestroyContext`, `sceNpTcsIntDestroyRequest`, `sceNpTcsIntUploadDataToNewSlot`, `sceNpTcsIntUploadDataToNewSlotEnd` and `sceNpTcsIntUploadDataToNewSlotWrite`, and the NID of each name matches. Signatures and behavior are unverified. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
