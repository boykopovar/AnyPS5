# libSceLibcInternal

## remove `m6uU37-b27s`

## add Functional

- [_ZSt9_LStrcollIwEiPKT_S2_S2_S2_PKSt8_Collvec](../../../core/libs/prx/libSceLibcInternal/WideCollate.cpp) (libSceLibcInternal, NID `m6uU37-b27s`) - not checked against `libSceLibcInternal.sprx` or on a console. Delegates to `sceLibcInternalWideCollateDoCompare_nid_no_patch` and ignores the `const _Collvec*` parameter, so non-C locale collation tables are not supported.
