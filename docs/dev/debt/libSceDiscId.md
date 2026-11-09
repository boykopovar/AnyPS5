# libSceDiscId technical debt

### Unknown function info

- [sceDiscIdGet, sceDiscIdGet2](../../../core/libs/prx/libSceDiscId/Export.cpp): `sceDiscIdGet` takes one output pointer; `sceDiscIdGet2` takes an output pointer and a 32-bit argument whose accepted values remain unresolved (it may be a bitfield). Both throw `NotImplemented_nid_no_patch` without reading or writing caller memory: the optical drive backend, the output structure layout, accepted flags and error returns remain unverified on hardware, so the unsupported behavior stays explicit.
