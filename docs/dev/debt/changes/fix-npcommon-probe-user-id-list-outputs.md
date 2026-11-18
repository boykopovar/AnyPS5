# libSceNpCommon

## add Unknown function info

- [sceNpUserGetUserIdList guest outputs](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) - the registered user ids are collected on the host, then the ids and the count are written with checked guest writes and `errno` preserved. A non-null unwritable list or count returns `0x80558009`, the error it already returns for a short list, instead of faulting the host; the count is not written when the list write fails. The console's result for unwritable pointers was not measured.
