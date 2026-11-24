# libSceVideodec2

## change [sceVideodec2TermLibrary and nine other exports]

- [sceVideodec2TermLibrary and seven other exports](../../../core/libs/prx/libSceVideodec2/Unimplemented.cpp) (libSceVideodec2) - report that they are not implemented; what they do on the console is not known. The others are `QueryLibraryInfo`, `QueryDecoderInfo`, `MapDirectMemory`, and `PresetDecoderConfigInfo` (plain, `Avc`, `Hevc`, `Vp9`). They are exported so that a title which imports them with `sceVideodec2InitLibrary` and calls none of them directly loads. Only `sceVideodec2MapDirectMemory` has a known parameter list, `(decoder, memory)`, from [EVO-PLAYER-PS5's `sce_videodec2.h`](https://github.com/sainsaji/EVO-PLAYER-PS5/blob/9715f6ee9bf9b53461d6fdf9a2e11b4c48357783/projects/evoplayer/media/include/sce/sce_videodec2.h#L173); the others take none here

## add Functional

- [sceVideodec2CreateDecoderBid and sceVideodec2MapMemory](../../../core/libs/prx/libSceVideodec2/Export.cpp) (libSceVideodec2) - `sceVideodec2CreateDecoderBid` follows the firmware 9.20 reading in #25862 (`resourceType == 1` returns `0x811D0203`; `bid == 1` creates a decoder; other bids throw `NotImplemented_nid_no_patch`), except null or unreadable/unwritable `config`, `memory`, or `handle` pointers throw `std::runtime_error` like `sceVideodec2CreateDecoder` rather than returning `0x811D0102`. `sceVideodec2MapMemory` validates the handle (`0x811D0103`), memory pointer (`0x811D0102`), and memory size (`0x811D0104`), returning 0 without using the memory (as the FFmpeg decoders ignore decoder memory) and without modelling `sceVdecCoreMapMemory` failures (`0x811D01FF`, `0x811D010C`).
