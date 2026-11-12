# libSceAjm technical debt

### Unknown function info

- [sceAjmGetFailedInstance, sceAjmInstanceGetSize, sceAjmTelemetryGetLog](../../../core/libs/prx/libSceAjm/Export.cpp) throw on every call. The pinned [PS5 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceajm-5217837f40.jsonl) lists them as defined exports of libSceAjm, so they stay there as parameterless import placeholders, under `_nid_disambig1` names that keep their NIDs. In firmware 13.42 `libSceAjm.sprx` lacks their NIDs and only `libSceAjm.native.sprx` exports them ([@BunnyFl's comparison](https://github.com/boykopovar/AnyPS5/issues/13805#issuecomment-6543803709)), where libSceAjm.native implements their argument checks. What 13.60's libSceAjm does with them wasn't checked.
