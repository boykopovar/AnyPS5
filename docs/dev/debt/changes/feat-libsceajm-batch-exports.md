# libSceAjm

## add Unknown function info

- [The batch, instance, memory and module exports](../../../core/libs/prx/libSceAjm/Export.cpp) (libSceAjm) forward to the implementations this module already compiles from libSceAjm.native, registered with `_nid_disambig1` symbols so the NID patcher resolves them. Names and NIDs follow the pinned [PS5 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceajm-5217837f40.jsonl); eight of its NIDs have no name in the database and remain unregistered. Behaviour, including the paths that still throw for the AJM driver, is that of libSceAjm.native and carries its gaps.
