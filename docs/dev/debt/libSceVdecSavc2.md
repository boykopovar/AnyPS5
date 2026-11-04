# libSceVdecSavc2 technical debt

### Unknown function info

- [sceSdecCreateSw2, sceSdecDestroySw2 and sceSdecQueryMemorySw2](../../../core/libs/prx/libSceVdecSavc2/Export.cpp) (libSceVdecSavc2) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecSavc2.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [sceSdecGetVersionSw2](../../../core/libs/prx/libSceVdecSavc2/Export.cpp) (libSceVdecSavc2) - the prototype and behaviour follow firmware 13.42's libSceVdecSavc2.sprx, read from disassembly and run under an emulator, not measured on a console: it takes a `uint32_t*`, writes 0x13590001 there unless the pointer is null, and returns nothing. The .native variant of the firmware module writes 0x06000000 instead and is not implemented here
