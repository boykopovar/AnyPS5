# libSceVideodec2

## add Functional

- [sceVideodec2MapDirectMemory](../../../core/libs/prx/libSceVideodec2/Export.cpp) (libSceVideodec2) - invalid arguments return the PS4 codes of [shadPS4 at 57e129ec](https://github.com/shadps4-emu/shadPS4/blob/57e129ec548cfcf8572c0a26b1c9aaf7e95dc3b6/src/core/libraries/videodec/videodec2.cpp#L308): `0x811D0103` for a null decoder, `0x811D0102` for a null or unreadable `memory`, `0x811D0101` for a `thisSize` other than 32, and `0x811D0104` for a null `memoryAddr` or zero `memorySize`, preserving `errno`. The codes follow `QueryComputeMemoryInfo` in the same library, which already returns `0x811D0102` and `0x811D0101` for the same class of validation; none is measured on a PS5. The accepted path performs no direct-memory mapping and still throws through `NotImplemented_nid_no_patch`, so no call with valid arguments is known to run. Closes when: measured on a PS5 with mapped direct memory.

## change [sceVideodec2TermLibrary and nine other exports]

- [sceVideodec2TermLibrary and eight other exports](../../../core/libs/prx/libSceVideodec2/Unimplemented.cpp) (libSceVideodec2) - report that they are not implemented; what they do on the console is not known. The others are `QueryLibraryInfo`, `QueryDecoderInfo`, `MapMemory`, `PresetDecoderConfigInfo` (plain, `Avc`, `Hevc`, `Vp9`) and `CreateDecoderBid`. They are exported so that a title which imports them with `sceVideodec2InitLibrary` and calls none of them directly loads. These take no arguments here. `sceVideodec2MapDirectMemory` now validates its arguments in [Export.cpp](../../../core/libs/prx/libSceVideodec2/Export.cpp), leaving only its accepted path unimplemented.
