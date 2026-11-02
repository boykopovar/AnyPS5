# libfontconfig technical debt

### Unknown function info

- [FcCharSet and FcMatrix exports](../../../core/libs/prx/libfontconfig/Export.cpp) (libfontconfig) - `FcCharSetCreate`, `FcCharSetDestroy`, `FcCharSetAddChar`, `FcCharSetMerge`, `FcCharSetSubtractCount` and `FcMatrixMultiply` follow the public [fontconfig.h](https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/2.12.6/fontconfig/fontconfig.h), [fccharset.c](https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/2.12.6/src/fccharset.c) and [fcmatrix.c](https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/2.12.6/src/fcmatrix.c). The module's fontconfig version is unknown and no console or title caller was read. Character sets hold no reference count and no constant state, because nothing exported here can share one yet.

### Callable exports

- [Callable exports](../../../core/libs/prx/libfontconfig/Export.cpp) - 47 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libfontconfig.c) and the [selected export metadata](https://gist.github.com/kyon-eth/7c1e54e6851733f72dbb507fdfbf35f4/206d5ec1b74f677f61a1eda755b490537209a8e1) from two version-labelled images (SHA-256 `3694ba82b2fbff0d5d789f91938906ebc8ee45262aaaba2b333d490f7e0c24ab`). The labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

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
| `FcPatternAddInteger` | `LNzAovAnOd8` |
| `FcLangSetAdd` | `Lsu4XJJ3V2o` |
| `FcFontSort` | `MApHziT-St4` |
| `FcPatternGetCharSet` | `NKrjlO4HBhg` |
| `FcPatternDel` | `Om2ww9GOmes` |
| `FcConfigDestroy` | `Oor7ukCXZOE` |
| `FcPatternReference` | `QZuEIDVT42s` |
| `FcObjectSetDestroy` | `WtTZKbENx4o` |
| `FcPatternGet` | `Xmsz9ffM-9g` |
| `FcPatternAddMatrix` | `Zqa5tqMhOrI` |
| `FcPatternAddString` | `bU30lh8YpkE` |
| `FcFontSetCreate` | `fJfx9D9g9W4` |
| `FcLangSetHasLang` | `gbtAblHLpQg` |
| `FcLangSetCreate` | `jjz2C1StauM` |
| `FcFreeTypeCharIndex` | `l240XTbw2vk` |
| `FcFontSetAdd` | `lEIEcYhU2vE` |
| `FcPatternDestroy` | `nVkAFM4iCS4` |
| `FcPatternAddCharSet` | `paHcj4DoPQ4` |
| `FcDefaultSubstitute` | `sqEJZ2yMVNw` |
| `FcObjectSetBuild` | `v5-8NZmvyrA` |
| `FcPatternAddLangSet` | `vqX-PYS3MPc` |
| `FcFontSetDestroy` | `vrZNehbMgsU` |
| `FcPatternGetLangSet` | `x55q-bC-i+8` |
| `FcPatternCreate` | `xGAEbwA7FNo` |
| `FcConfigReference` | `yQed09jA3VA` |
| `FcPatternFormat` | `zqe0Dw9BCpc` |
