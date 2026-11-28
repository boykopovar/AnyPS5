# libSceAjm technical debt

### Unknown function info

- [sceAjmGetFailedInstance, sceAjmInstanceGetSize, sceAjmTelemetryGetLog](../../../core/libs/prx/libSceAjm/Export.cpp) - follow libSceAjm.native's argument checks (@BunnyFl's 13.42 run and disassembly). Pinned [PS5 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceajm-5217837f40.jsonl) lists them as defined exports of libSceAjm under `_nid_disambig1` names. In 13.42 `libSceAjm.sprx` lacked their NIDs and only `libSceAjm.native.sprx` exported them ([comparison](https://github.com/boykopovar/AnyPS5/issues/13805#issuecomment-6543803709)). Each validates arguments (0x8093000C for invalid flags, 0x80930005 for null outputs) before requesting the `/dev/ajm` kernel driver, which is unmodeled and throws. Driver responses and failed ioctl error mappings are not modeled.
