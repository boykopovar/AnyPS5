# libfontconfig technical debt

### Functional set operations

- [Font sets and object sets](../../../core/libs/prx/libfontconfig/Sets.cpp) follow Fontconfig 2.15.0 [fcfs.c](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fcfs.c) and [fclist.c](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fclist.c) for ownership, copies, and allocations in blocks of 32 fonts or four object names. Console ABI and version are unverified. Native tests cover 300 patterns and duplicate names. `FcFontSetAdd(NULL, ...)` returns false rather than dereferencing null. Object-name order depends on pointer allocation addresses and is not portable; only membership is compared.
- [Populated font-set destruction](../../../core/libs/prx/libfontconfig/Sets.cpp) calls `FcPatternDestroy`, which remains an unconditional throwing stub in this branch. [PR #14435](https://github.com/boykopovar/AnyPS5/pull/14435) implements the pattern destructor; until it lands, only empty-set destruction is usable at runtime. The isolated tests inject a typed destructor to check ownership.

### Callable exports

- [Remaining callable exports](../../../core/libs/prx/libfontconfig/Export.cpp) - 48 of the 53 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libfontconfig.c) and the [selected export metadata](https://gist.github.com/kyon-eth/7c1e54e6851733f72dbb507fdfbf35f4/206d5ec1b74f677f61a1eda755b490537209a8e1) from two version-labelled images (SHA-256 `3694ba82b2fbff0d5d789f91938906ebc8ee45262aaaba2b333d490f7e0c24ab`). The labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `FcLangSetDestroy` | `+6aV1q-CAIg` |
| `FcPatternRemove` | `+XnLLwt2q-Y` |
| `FcPatternGetInteger` | `2BFkLGmS+Ao` |
| `FcConfigGetCurrent` | `2M54M-SKV-k` |
| `FcPatternGetBool` | `3zWpyQga5N0` |
| `FcPatternAddBool` | `6LGRGSLdSdQ` |
| `FcFontList` | `7+Gq-0xFybo` |
| `FcFontSetMatch` | `8DLBHeMsS1k` |
| `FcConfigSubstitute` | `8v4Xk2b+C1k` |
| `FcPatternGetMatrix` | `9x04OJBX0J0` |
| `FcInit` | `AwGzFwnaitQ` |
| `FcInitBringUptoDate` | `Ck9k-L7rz-I` |
| `FcPatternGetString` | `CkZ778Hu6Ng` |
| `FcFontRenderPrepare` | `CvqQqWqcO-0` |
| `FcPatternAddDouble` | `D-WphAs+Lh8` |
| `FcPatternDuplicate` | `DPxdK61lRaE` |
| `FcFontMatch` | `FqneT2syjGE` |
| `FcPatternFilter` | `H9TOtqlqmHM` |
| `FcConfigCreate` | `HGtDYp4ddNE` |
| `FcPatternGetFTFace` | `HjlABSyLzNg` |
| `FcPatternEqual` | `HxFAszDPXuE` |
| `FcCharSetSubtractCount` | `J8GcxU03WkE` |
| `FcPatternAddInteger` | `LNzAovAnOd8` |
| `FcLangSetAdd` | `Lsu4XJJ3V2o` |
| `FcFontSort` | `MApHziT-St4` |
| `FcCharSetMerge` | `MafDsPXgY6k` |
| `FcPatternGetCharSet` | `NKrjlO4HBhg` |
| `FcCharSetDestroy` | `Nw0ilSm8FxY` |
| `FcPatternDel` | `Om2ww9GOmes` |
| `FcConfigDestroy` | `Oor7ukCXZOE` |
| `FcPatternReference` | `QZuEIDVT42s` |
| `FcPatternGet` | `Xmsz9ffM-9g` |
| `FcPatternAddMatrix` | `Zqa5tqMhOrI` |
| `FcPatternAddString` | `bU30lh8YpkE` |
| `FcLangSetHasLang` | `gbtAblHLpQg` |
| `FcCharSetCreate` | `hNyN1seFzWY` |
| `FcLangSetCreate` | `jjz2C1StauM` |
| `FcFreeTypeCharIndex` | `l240XTbw2vk` |
| `FcPatternDestroy` | `nVkAFM4iCS4` |
| `FcCharSetAddChar` | `nXYd1oE9ZM0` |
| `FcMatrixMultiply` | `oM+8JNTrd24` |
| `FcPatternAddCharSet` | `paHcj4DoPQ4` |
| `FcDefaultSubstitute` | `sqEJZ2yMVNw` |
| `FcPatternAddLangSet` | `vqX-PYS3MPc` |
| `FcPatternGetLangSet` | `x55q-bC-i+8` |
| `FcPatternCreate` | `xGAEbwA7FNo` |
| `FcConfigReference` | `yQed09jA3VA` |
| `FcPatternFormat` | `zqe0Dw9BCpc` |
