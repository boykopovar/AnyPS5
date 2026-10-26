# libSceVdecShevc technical debt

### Unknown function info

- [sceSdecCreateSwHevc, sceSdecDestroySwHevc, sceSdecExecuteSwHevc, sceSdecGetFrameSwHevc, sceSdecGetVersionSwHevc, sceSdecQueryMemorySwHevc and sceSdecResetSwHevc](../../../core/libs/prx/libSceVdecShevc/Export.cpp) (libSceVdecShevc) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecShevc.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
