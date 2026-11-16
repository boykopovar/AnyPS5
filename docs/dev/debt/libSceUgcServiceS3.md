# libSceUgcServiceS3 technical debt

### Unknown function info

- [14 exports (10 without a known name, bound by NID)](../../../core/libs/prx/libSceUgcServiceS3/Export.cpp) (libSceUgcServiceS3) - every function export of `libSceUgcServiceS3.sprx` from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown apart from the checks under Functional; past them every export throws through `NotImplemented_nid_no_patch`, and the parameterless declarations are import-resolution placeholders

### Functional

- [sceUgcServiceS3CreateRequest, sceUgcServiceS3CancelRequest, sceUgcServiceS3UploadFileWithGmsResponse, sceUgcServiceS3DownloadFileWithGmsResponse, sceUgcServiceS3Unknown01 and sceUgcServiceS3Unknown02 to sceUgcServiceS3Unknown09](../../../core/libs/prx/libSceUgcServiceS3/Export.cpp) (libSceUgcServiceS3) - only the answer before the module's client object exists is implemented, from firmware 13.42's disassembly, not measured on a console. The object pointer starts zeroed and only `sceUgcServiceS3Unknown00` stores one, which still throws here, so no object ever exists. Without it `sceUgcServiceS3Unknown01` returns 0 and the other twelve return `0x81b70002` before reading any argument.
