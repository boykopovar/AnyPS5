# libSceDiscMap

## remove `fJgP+wqifno`

## remove `5+vOlukvkfg`

## change [libSceDiscMap]

- [libSceDiscMap](../../../core/libs/prx/libSceDiscMap/Export.cpp): loading disc bitmap metadata and resolving file byte ranges to bitmap blocks are not implemented. `sceDiscMapIsRequestOnHDD`, `ioKMruft1ek`, `5+vOlukvkfg` and `sceDiscMapGetPackageSize` implement the confirmed no-bitmap path: return `SCE_DISC_MAP_ERROR_NO_BITMAP_INFO` (`0x81100004`) before validating any argument and leave outputs unchanged. The supplied module confirms signed 64-bit byte offset/size, a 32-bit `IsRequestOnHDD` result and three 64-bit outputs for `ioKMruft1ek`. A PC run of the firmware 13.42 module on x86-64 (#8647 comment 6416230506, not a console measurement) confirms `sceDiscMapGetPackageSize` returns `0x81100004` before argument checks while its bitmap-state word is clear; its loaded state and bitmap loading remain unimplemented. `fJgP+wqifno` returns the same error in the same way, resting only on a general note about the PS4 module (see its Unknown function info entry).

## remove [Additional callable exports]

## add Unknown function info

- [sceDiscMapUnknown_fJgP_plus_wqifno](../../../core/libs/prx/libSceDiscMap/Export.cpp) (libSceDiscMap, NID `fJgP+wqifno`) - no name is known; not measured on a console. Returns `SCE_DISC_MAP_ERROR_NO_BITMAP_INFO` without reading or writing its arguments, like its three siblings; [shadPS4#2686](https://github.com/shadps4-emu/shadPS4/pull/2686) reports from Ghidra that every call seems to check an init global first and return this error while it is clear. That same pull request gives this function 0 with zeroed outputs, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/741d9b75fd87104ae2f81e4443ffc5433828eebf/src/core/libraries/disc_map/disc_map.cpp#L20-L26) and [fpPS4](https://github.com/red-prig/fpPS4/blob/04cefd43e6fddd1ab033e7980cd356d14c964905/src/ps4_libscediscmap.pas#L38-L51) still do. #10531 reads CUSA00004's libSceFios2 as passing three 64-bit outputs (LBA package location, LBA start, count) and keeping the request unmapped after it. Closes when: measured on a console.
- [libSceDiscMapUnknown_5_plus_vOlukvkfg_nid_no_patch](../../../core/libs/prx/libSceDiscMap/Export.cpp) (libSceDiscMap, NID `5+vOlukvkfg`) - name unknown; read from the disassembly of `libSceDiscMap.sprx` (firmware 13.40), not measured on a console. The module tests the same bitmap-state word as `ioKMruft1ek` and `sceDiscMapGetPackageSize` first and returns `0x81100004` while it is clear, without reading its arguments; that is the only path implemented. With a bitmap loaded it stores its third argument (a 64-bit size) through its fourth, returns `0x81100001` when the first argument has bits other than 0 and 8 set, and otherwise looks up the offset and size given by its second and third arguments in the bitmap
