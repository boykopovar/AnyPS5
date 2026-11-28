# libSceLibcInternal

## change [_ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv]

- [sync](../../../core/libs/prx/libSceLibcInternal/Filebuf.cpp) (libSceLibcInternal, NID `+WvmZi3216M`) - declared from its mangled name with `self` and an `int` return, and implemented: a null `FILE*` at 0xa8 returns 0, otherwise the buffer vtable `overflow` slot is called with eof and `fflush` on the stream decides 0 or -1, in the order and returns of Microsoft STL [__msvc_filebuf.hpp#L626-L632](https://github.com/microsoft/STL/blob/770da5aa457af11f0a796600e1aa6f7f6fce14a6/stl/inc/__msvc_filebuf.hpp#L626-L632). Older Dinkumware `sync` instead undoes putback and clears `_Wrotesome` and open mode, whose guest offsets no code here proves, so that form is unverified.
