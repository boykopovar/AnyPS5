# libSceFlexContent technical debt

### Unknown function info

- [12 exports (5 without a known name, bound by NID)](../../../core/libs/prx/libSceFlexContent/Export.cpp) (libSceFlexContent) - every function export of `libSceFlexContent.sprx` from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export not covered by the next entry throws through `NotImplemented_nid_no_patch`
- [sceFlexContentTerminate, sceFlexContentMount, sceFlexContentGetDownloadProgress, sceFlexContentDelete, sceFlexContentGetLatestVersion, sceFlexContentGetInstalledVersion and the 5 NID-bound exports](../../../core/libs/prx/libSceFlexContent/Export.cpp) (libSceFlexContent) - they return `0x81B60001` and nothing else. In firmware 13.42 each first tests the library's IPMI client and returns `0x81B60001` while it does not exist; only `sceFlexContentInitialize` creates it, and it still throws here. Once it is implemented, these must test the client and implement what follows. Read from the module's disassembly, not measured on a console.
