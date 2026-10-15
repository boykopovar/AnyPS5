# libSceContentDelete technical debt

### Unknown function info

- [libSceContentDelete](../../../core/libs/prx/libSceContentDelete/Export.cpp) (libSceContentDelete) - content delete memory and status interfaces; stubs throw until content deletion subsystem is implemented
- [sceContentDeleteById](../../../core/libs/prx/libSceContentDelete/Export.cpp) (libSceContentDelete) - imported by PPSA06465, which fails to load without it; added without an implementation because its signature is unknown
