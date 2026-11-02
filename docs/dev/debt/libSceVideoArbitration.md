# libSceVideoArbitration technical debt

### Functional

- [sceVideoArbitrationInitialize](../../../core/libs/prx/libSceVideoArbitration/Export.cpp) (libSceVideoArbitration) tracks initialization atomically; duplicate calls throw `std::logic_error`. Uninitialized calls to `sceVideoArbitrationEnable` and `sceVideoArbitrationAcceptEvent` throw `std::logic_error`. Initialized calls dispatch to `NotImplemented_nid_no_patch`.

### Unknown function info

- [sceVideoArbitrationAcceptEvent and sceVideoArbitrationEnable](../../../core/libs/prx/libSceVideoArbitration/Export.cpp) (libSceVideoArbitration) - names and NIDs are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVideoArbitration.c), but no credible prototypes were found. Parameterless declarations resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified.
