# libc

## add Functional

- [libc++ string conversions](../../../core/libs/prx/libc/src/LibcxxSto.cpp) (libc) implement Azahar’s `stoi`, `stoul`, `stoull` and `stof` imports (#5927). They parse the guest string with the existing libc conversion functions, preserve guest errno and report an LP64 consumed-byte index. Full title and firmware behavior are unmeasured.
