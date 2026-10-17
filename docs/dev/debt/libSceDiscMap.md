# libSceDiscMap technical debt

### Silent stubs

- [libSceDiscMap](../../../core/libs/prx/libSceDiscMap/Export.cpp) implements only the state with no disc bitmap: `sceDiscMapIsRequestOnHDD` and `ioKMruft1ek` return `SCE_DISC_MAP_ERROR_NO_BITMAP_INFO` (0x81100004) for every request and write nothing. CUSA02636's bundled `libSceFios2.prx` imports both; what it does with the error is not known (shadPS4 returns the same, and [shadps4-game-compatibility #617](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/617) lists CUSA02636 as in-game)

### Unknown function info

- [sceDiscMapIsRequestOnHDD, sceDiscMapUnknown_ioKMruft1ek](../../../core/libs/prx/libSceDiscMap/Export.cpp) (libSceDiscMap, NIDs `lbQKqsERhtE` and `ioKMruft1ek`) - `ioKMruft1ek` has no known name. The `(path, offset, size, result)` and `(path, offset, size, flags, out1, out2)` signatures follow [shadPS4 disc_map.cpp](https://github.com/shadps4-emu/shadPS4/blob/b2d7ec7d31f9ad9426db93afe079db8679b3e1e0/src/core/libraries/disc_map/disc_map.cpp); the widths of the `ioKMruft1ek` outputs are unknown (shadPS4 `int*`, [fpPS4](https://github.com/red-prig/fpPS4/blob/04cefd43e6fddd1ab033e7980cd356d14c964905/src/ps4_libscediscmap.pas) `PInt64`), so they are `void*`. The error code is the one both return; [shadPS4 #2686](https://github.com/shadps4-emu/shadPS4/pull/2686) reads the console library as checking an initialisation global first in each call and returning it while the global is clear. Not measured on a console
