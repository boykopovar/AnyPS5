# libSceOpusCeltDec

## remove sceOpusCeltDecCreate

## add Functional

- [sceOpusCeltDecCreate](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp) (libSceOpusCeltDec) - creates a standalone CELT decoder without an AJM context while preserving errno; sample rates and channel counts are validated against libopus requirements. Context ownership and AJM hardware error codes remain unmodeled.
