# libSceLibcInternal

## remove `X1DNtCe22Ks`

## add Functional

- [_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec](../../../core/libs/prx/libSceLibcInternal/Collate.cpp) (libSceLibcInternal, NID `X1DNtCe22Ks`) - not checked against `libSceLibcInternal.sprx` or on a console. Delegates to `sceLibcInternalCollateDoCompare_nid_no_patch` and ignores the `const _Collvec*` parameter, so non-C locale collation tables are not supported; the wide `_LStrcoll` helper still throws.
