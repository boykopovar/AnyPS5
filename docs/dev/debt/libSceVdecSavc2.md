# libSceVdecSavc2 technical debt

### Unknown function info

- [sceSdecCreateSw2, sceSdecDestroySw2, sceSdecGetVersionSw2 and sceSdecQueryMemorySw2](../../../core/libs/prx/libSceVdecSavc2/Export.cpp) (libSceVdecSavc2) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecSavc2.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
