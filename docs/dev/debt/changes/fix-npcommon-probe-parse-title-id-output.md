# libSceNpCommon

## add Unknown function info

- [sceNpUtilParseTitleId guest output](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) - the communication id is built on the host and written with one checked guest write, with `errno` preserved. A non-null unwritable output returns `0x80550601`, the error it already returns for a null title id, instead of faulting the host. The title id string is still read in place. The console's result for unwritable pointers was not measured.
