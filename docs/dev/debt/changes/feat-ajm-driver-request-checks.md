# libSceAjm.native

## remove [sceAjmBatchGetLog, sceAjmInstanceAvailable]

## remove [sceAjmInstanceExtend, sceAjmInstanceSwitch]

## add Unknown function info

- [sceAjmInstanceExtend, sceAjmInstanceSwitch, sceAjmInstanceAvailable and sceAjmBatchGetLog](../../../core/libs/prx/libSceAjm.native/src/DriverQueries.cpp) (libSceAjm.native) - follow firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21856#issuecomment-6939955028), disassembly only, not run on a console): each checks its arguments, then returns the answer of one `ioctl` on the AJM device, which throws here as for `InstanceGetSize`. `Extend` and `Switch(context, codec, uint64_t flags, instance)` and `Available(context, codec, flags)`, with no fourth argument, return 0x8093000C for the flags `InstanceGetSize` refuses. `BatchGetLog(context, uint32_t size, void* buffer, void* out)` returns 0x80930005 for a size above 5000 or a null `buffer` or `out`

## add Functional

- [sceAjmDecWVorbisCreateSetupPacket](../../../core/libs/prx/libSceAjm.native/Export.cpp) (libSceAjmWVorbis, implemented in libSceAjm.native) - makes firmware 13.42's checks ([reading](https://github.com/boykopovar/AnyPS5/issues/21856#issuecomment-6939955028), disassembly only, not run on a console): `0x8A6E0001` for a null parameter, `0x8A6E0002` unless its `size` is `0x38`, then `0x8A6E0001` for negative `channels` or a null `input` or `output`. Past them the console's setup-packet builder runs, which wasn't read beyond its first bytes, so a call that passes them throws
