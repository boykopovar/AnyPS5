# libSceDiscId technical debt

### Unknown function info

- [sceDiscIdGet](../../../core/libs/prx/libSceDiscId/Export.cpp) takes one output pointer; `sceDiscIdGet2` takes an output pointer and a 32-bit argument that must be zero. Both pass the pointer to a device ioctl without accessing its contents. Output structure sizes and fields require the device-side implementation or a caller and remain unresolved.
