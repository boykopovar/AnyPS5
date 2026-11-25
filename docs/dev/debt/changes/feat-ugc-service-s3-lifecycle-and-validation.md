# libSceUgcServiceS3

## add Functional

- [libSceUgcServiceS3 exports](../../../core/libs/prx/libSceUgcServiceS3/Export.cpp) - implemented firmware 13.60 client lifecycle (`sceUgcServiceS3Unknown00`/`sceUgcServiceS3Unknown01`), sequential request ID map (`sceUgcServiceS3CreateRequest`, `sceUgcServiceS3Unknown02` delete, `sceUgcServiceS3CancelRequest`), callback/user-data/progress accessors (`sceUgcServiceS3Unknown03` to `sceUgcServiceS3Unknown06`), `sceUgcServiceS3Unknown09` empty-info guard (`0x81b71006`), and upload/download parameter, part-index (`<= 0x2710`), and file-type validation (`0x81b70004`, `0x81b71001`, `0x81b71005`) before resetting progress; `outInfo`/`outSize` are not populated and S3 HTTP/JSON transfer steps after validation still call `NotImplemented_nid_no_patch`.
