# libSceVideodec

## change [sceVideodecMapMemory]

- [sceVideodecMapMemory](../../../core/libs/prx/libSceVideodec/Export.cpp) (libSceVideodec) - throws after its argument checks: what the decoder's mapping call does and the result it gives are unknown. The checks follow firmware 13.42's `libSceVideodec.sprx` (read from disassembly, not run on a console); the `printf` line the module prints on each failed check is not reproduced, and a block whose checked fields cannot be read returns the argument pointer error where the console faults.

## change [sceVideodec pointer safety]

- [sceVideodec pointer safety](../../../core/libs/prx/libSceVideodec/Export.cpp) (libSceVideodec) - `sceVideodecCreateDecoder`, `sceVideodecDecode`, `sceVideodecDeleteDecoder`, `sceVideodecFlush`, `sceVideodecMapMemory`, `sceVideodecReset`, and `sceVideodecQueryResourceInfo` validate caller input and output pointers using `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`, returning `VideodecErrorArgumentPointer` or `VideodecErrorFrameBufferPointer` on invalid or unwritable memory rather than causing host access violations. What the console returns for an unreadable or unwritable pointer is not measured; these functions return the pointer errors instead.
