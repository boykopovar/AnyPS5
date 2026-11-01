# libSceAjm technical debt

### Unknown function info

- [sceAjmGetFailedInstance, sceAjmInstanceGetSize, sceAjmTelemetryGetLog](../../../core/libs/prx/libSceAjm/Export.cpp) throw on every call. Their names and NIDs are confirmed defined function exports in the pinned [PS5 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceajm-5217837f40.jsonl); signatures and behavior remain unknown. Parameterless `int` declarations are import placeholders, not verified prototypes or working implementations.
