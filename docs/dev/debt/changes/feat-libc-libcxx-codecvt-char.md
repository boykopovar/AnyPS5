# libc

## add Functional

- [std::__1::codecvt<char,char,__mbstate_t> conversion methods](../../../core/libs/prx/libc/src/CodecvtUtf.cpp) (libc) - the six exact method imports listed for Azahar in RetroArch (PPSA99169, #5927) follow [LLVM libc++'s no-conversion specialization](https://github.com/llvm/llvm-project/blob/main/libcxx/src/locale.cpp#L1131-L1176). `do_in`, `do_out` and `do_unshift` report `noconv` and leave caller buffers untouched; length and encoding use one byte per character. The guest facet object is not read. The console firmware provider and behavior have not been measured.
