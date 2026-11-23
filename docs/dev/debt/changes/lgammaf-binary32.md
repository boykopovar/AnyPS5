# libc

## add Functional

- [lgammaf, lgammaf_r, gammaf and gammaf_r](../../../core/libs/prx/libc/src/Math.cpp) (libc) use the binary32 [FreeBSD msun algorithm](https://github.com/freebsd/freebsd-src/blob/4e26149e5c0c0cd171a9ea428eafec0c80946c30/lib/msun/src/e_lgammaf_r.c) with fused evaluation measured in the libraries bundled with PPSA26344 and PPSA28050. Other library revisions, physical-console results and enabled exception traps remain unverified.
