# libSceEditMp4 technical debt

### Unknown function info

- [sceEditMp4Abort, sceEditMp4Edit, sceEditMp4GetStatus, sceEditMp4Initialize, sceEditMp4InitializeParam, sceEditMp4QueryMemSize, sceEditMp4SetFinishCallback, sceEditMp4Terminate and sceEditMp4Unite](../../../core/libs/prx/libSceEditMp4/Export.cpp) (libSceEditMp4) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceEditMp4.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
