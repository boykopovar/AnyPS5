# libScePlayReady2 technical debt

### Unknown function info

- [scePlayReadyBufferInitWithData, scePlayReadyCloneDecryptContext, scePlayReadyDebugPrintf, scePlayReadyDebugSetLevel, scePlayReadyLicenseDelete, scePlayReadyLicenseDeleteInMemory, scePlayReadyLicenseGetExpirationTime, scePlayReadyLicenseGetProperty, scePlayReadyReaderClose, scePlayReadyReaderDecryptMultiple, scePlayReadyReaderDecryptMultipleOpaque, scePlayReadyReaderDecryptOpaque, scePlayReadyReaderDecrypt_Legacy, scePlayReadyReaderFreeOpaqueContent, scePlayReadyReaderRetrieveOpaqueContent and scePlayReadyReinitialize](../../../core/libs/prx/libScePlayReady2/Export.cpp) (libScePlayReady2) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePlayReady2.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Inventory exports declared elsewhere
 The PS5 3.20 export table for this module also contains 39 names already declared under libScePlayReady in this tree; the duplicate-export rule keeps each name in a single library, so they are omitted here. They resolve through the sibling declarations.
