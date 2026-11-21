# libScePlayReady2 technical debt

### Unknown function info

- [scePlayReadyCloneDecryptContext, scePlayReadyDebugPrintf, scePlayReadyDebugSetLevel, scePlayReadyLicenseDelete, scePlayReadyLicenseDeleteInMemory, scePlayReadyLicenseGetExpirationTime, scePlayReadyLicenseGetProperty, scePlayReadyReaderClose, scePlayReadyReaderDecryptMultiple, scePlayReadyReaderDecryptMultipleOpaque, scePlayReadyReaderDecryptOpaque, scePlayReadyReaderDecrypt_Legacy, scePlayReadyReaderFreeOpaqueContent, scePlayReadyReaderRetrieveOpaqueContent and scePlayReadyReinitialize](../../../core/libs/prx/libScePlayReady2/Export.cpp) (libScePlayReady2) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePlayReady2.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Functional

- [scePlayReadyApiSetInitParamFlags](../../../core/libs/prx/libScePlayReady2/Export.cpp) (libScePlayReady2) - follows firmware 13.42's libScePlayReady2.sprx, read from disassembly, not measured on a console: it returns `0x80070057` unless the block at the first argument holds 0x50524459 at offset 0, 3 and 0x40 as 16-bit values at offsets 4 and 6, 0 at offsets 0xC (32-bit) and 0x10 (64-bit) and non-zero 64-bit values at offsets 0x18 to 0x30, checked in that order, and otherwise stores the 32-bit second argument at offset 8 and returns 0. A field that cannot be read, or a destination that cannot be written, throws.
- [scePlayReadyBufferInitWithData](../../../core/libs/prx/libScePlayReady2/Export.cpp) (libScePlayReady2) - only the argument checks firmware 13.42 makes before its first call are implemented, from its disassembly, not measured on a console: `0x80070057` for a null first argument, or a null second argument with a non-zero third or the reverse. Past them it reads the module's initialization flag, which only the module's initialization routine sets, so it throws.

### Inventory exports declared elsewhere
 The PS5 3.20 export table for this module also contains 39 names already declared under libScePlayReady in this tree; the duplicate-export rule keeps each name in a single library, so they are omitted here. They resolve through the sibling declarations.
