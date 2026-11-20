# libSceNpCommon

## add Unknown function info

- [sceNpStrBuildHex guest buffers](../../../core/libs/prx/libSceNpCommon/Strings.cpp) - the source is copied in and the hex text copied out in 256-byte blocks with checked guest copies, then the terminator is written, in the same order as before. An unreadable source or unwritable destination returns `0x80558009` (invalid argument), as the integer-to-string exports do, instead of faulting the host; the blocks already written stay written. The console's result for unmapped pointers was not measured.
