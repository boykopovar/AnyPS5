# libSceVideodec2

## add Functional

- [sceVideodec2DecodeFirstSlice, sceVideodec2DecodeNextSlice, and sceVideodec2SyncAllSlice](../../../core/libs/prx/libSceVideodec2/InventoryExports.cpp) - Verified against firmware 13.60 disassembly: all three validate decoder magic `0xa824d9799010a455` at `+0x68` (`0x811d0103`), guest pointers (`0x811d0102`), and struct sizes (`0x40`, `0x20`, `0x38`, `0x18` -> `0x811d0101`). `DecodeFirstSlice` and `DecodeNextSlice` check slice input fields and zeroed bytes `0x39..0x3f` (`0x811d01e0`); `DecodeFirstSlice` checks frame buffer pointer (`0x811d0107`), 256-byte alignment (`0x811d0108`), and non-zero size (`0x811d0106`). `SyncAllSlice` clears `OutputInfo::isValid` before checking sync result size. Post-gate `libSceVdecCore` calls still throw via `NotImplemented_nid_no_patch`.
