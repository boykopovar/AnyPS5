# libSceLibcInternal

## change [_ZNSt13basic_filebufIwSt11char_traitsIwEE7_UnlockEv]

- [basic_filebuf<char>::_Unlock, basic_filebuf<wchar_t>::_Unlock](../../../core/libs/prx/libSceLibcInternal/Filebuf.cpp) (libSceLibcInternal, NIDs `yofHspnD9us`, `ryl2DYMxlZ0`) - the object layout and lock behavior are read from `libc.prx`, SDK 10.00, at 0x84580 and 0xb4b00, not measured on a console or verified on newer SDKs. Both read the file pointer at offset 0xa8 and call `_Unlockfilelock` (0x3790) only when it is non-null; `target/include/fstream:310-313` and `target/include/stdio.h:240,342` agree on the void member signature and callee. The existing guest stream backend supplies the lock. An unreadable or wrapping object address throws instead of reproducing a native memory fault.

## remove [_ZNSt13basic_filebufIcSt11char_traitsIcEE7_UnlockEv]
