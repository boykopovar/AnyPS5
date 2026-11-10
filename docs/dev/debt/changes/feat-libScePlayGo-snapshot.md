# libScePlayGo

## remove scePlayGoSnapshot

## add Silent stubs

- [scePlayGoSnapshot](../../../core/libs/prx/libScePlayGo/Export.cpp) (libScePlayGo) - validates handle and null filename pointer, preserves errno, and returns 0 without writing snapshot file data; parameter list follows pgo_stub and rps5e; Sony console snapshot format is unmeasured and writing nothing preserves title progression and host storage.
