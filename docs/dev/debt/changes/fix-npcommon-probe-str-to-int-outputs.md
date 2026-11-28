# libSceNpCommon

## add Unknown function info

- [String-to-integer guest outputs](../../../core/libs/prx/libSceNpCommon/Strings.cpp) - `sceNpStrToInt32`, `sceNpStrToInt64`, `sceNpStrToUInt32` and `sceNpStrToUInt64` write the optional parsed length and then the value with checked guest writes, in the same order as before. A non-null unwritable output returns `0x80558009` (invalid argument), as the integer-to-string exports do, instead of faulting the host. The source string is still read in place. The console's result for unwritable pointers was not measured.
