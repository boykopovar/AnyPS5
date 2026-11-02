# libSceVdecSavc technical debt

### Unknown function info

- [sceSdecCreateSw, sceSdecDestroySw and sceSdecQueryMemorySw](../../../core/libs/prx/libSceVdecSavc/Export.cpp) (libSceVdecSavc) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecSavc.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [sceSdecGetVersionSw](../../../core/libs/prx/libSceVdecSavc/Export.cpp) (libSceVdecSavc) - the prototype and behaviour follow firmware 13.42's libSceVdecSavc.sprx, read from disassembly and run under an emulator, not measured on a console: it takes a `uint32_t*`, writes 0 there unless the pointer is null, and returns nothing
