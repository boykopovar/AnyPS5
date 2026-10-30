# libScePlayReady2 technical debt

### Unknown function info

- [13.60 inventory additions](../../../core/libs/prx/libScePlayReady2/Export.cpp) - `scePlayReadyApiSetInitParamFlags` (`O24y38B4uqY`) have unknown signatures and behavior. The parameterless `int` declarations only resolve imports and are not verified ABI prototypes; every call throws through `NotImplemented_nid_no_patch`. Name/NID bindings are from the [13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceplayready2-cf140a4c7d.jsonl).

- [scePlayReadyBufferInitWithData, scePlayReadyCloneDecryptContext, scePlayReadyDebugPrintf, scePlayReadyDebugSetLevel, scePlayReadyLicenseDelete, scePlayReadyLicenseDeleteInMemory, scePlayReadyLicenseGetExpirationTime, scePlayReadyLicenseGetProperty, scePlayReadyReaderClose, scePlayReadyReaderDecryptMultiple, scePlayReadyReaderDecryptMultipleOpaque, scePlayReadyReaderDecryptOpaque, scePlayReadyReaderDecrypt_Legacy, scePlayReadyReaderFreeOpaqueContent, scePlayReadyReaderRetrieveOpaqueContent and scePlayReadyReinitialize](../../../core/libs/prx/libScePlayReady2/Export.cpp) (libScePlayReady2) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePlayReady2.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Inventory exports declared elsewhere
 The PS5 3.20 export table for this module also contains 39 names already declared under libScePlayReady in this tree; the duplicate-export rule keeps each name in a single library, so they are omitted here. They resolve through the sibling declarations.
