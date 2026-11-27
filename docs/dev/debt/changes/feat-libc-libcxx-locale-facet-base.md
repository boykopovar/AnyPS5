# libc

## add Functional

- [std::__1 locale::facet and locale::id](../../../core/libs/prx/libc/src/LibcxxLocaleFacet.cpp) (libc) - the std::__1 locale class itself (constructors, `classic`, `use_facet`, `has_facet`, `__install_ctor`) is not provided, so a title that builds a locale still fails at import. Facet `D2`, `__on_zero_shared`, `id::__get`, the eight facet ids and the facet typeinfo, with the [codecvt<char,char> `D2` and typeinfo](../../../core/libs/prx/libc/src/CodecvtUtf.cpp), follow [LLVM libc++ 17 locale.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/locale.cpp), with ids numbered from 0 in first-use order. A null or misaligned id, an id whose once flag holds another value, and a null facet throw, where libc++ returns or crashes. The `__shared_count` and `codecvt_base` typeinfo are private records. The console provider, its id numbering and its typeinfo layout are unmeasured.
