# shader

## change [MUBUF loads to LDS]

- [MUBUF loads to LDS](../../../core/shader/recompiler/Translation/src/MemoryInstructions/BufferInstructions.cpp) - gfx1013 is not measured. `buffer_load_ubyte`, `sbyte`, `ushort`, `sshort`, `dword` and `format_x` with LDS=1 are measured on RDNA2 (AMD Radeon 1002:13c0, gfx1036, driver 32.0.21043.5001) through the [hardware oracle](../HW_ORACLE.md), wave32 and wave64, IEEE 0, DX10 clamp 1, denorm32 0, denorm16 3, round 0, FP16 overflow 0: each active lane writes the dword its VGPR form returns (0 past `NUM_RECORDS`) at `M0[15:0] + OFFSET + 4 * lane` with the low two bits dropped, nothing at or past the allocation or wrapping at 64 KiB; no VGPR is written and VDATA is ignored. SOFFSET and vector addressing affect only the buffer.

## add Unknown function info

- [MUBUF loads to LDS, refused forms](../../../core/shader/recompiler/RdnaDecoder/src/RdnaMemoryOpDecoder.cpp) - the D16, multi-dword and `format_xy`/`xyz`/`xyzw` loads with LDS=1 throw because they are not measured. TFE with LDS=1 throws: the VGPRs stayed unchanged, but the same TFE bit without LDS wrote no status dword either, so its meaning is unclear. The execution tests [MubufLoadLds](../../../core/libs/prx/libSceAgcDriver/tests/execution/MubufLoadLds.cpp) and [MubufLoadLdsSubword](../../../core/libs/prx/libSceAgcDriver/tests/execution/MubufLoadLdsSubword.cpp) skip on hosts whose subgroups hold fewer than 32 lanes.
