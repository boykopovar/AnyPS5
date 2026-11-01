# libSceAjm technical debt

### Unknown function info

- [sceAjmGetFailedInstance, sceAjmInstanceGetSize and sceAjmTelemetryGetLog](../../../core/libs/prx/libSceAjm.native/src/DriverQueries.cpp) are exported by libSceAjm.native only: in firmware 13.42 `libSceAjm.sprx` doesn't have their NIDs ([@BunnyFl's comparison](https://github.com/boykopovar/AnyPS5/issues/13805#issuecomment-6543803709)). The [PS5 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceajm-5217837f40.jsonl) lists them under libSceAjm, and whether 13.60's libSceAjm exports them wasn't checked.
