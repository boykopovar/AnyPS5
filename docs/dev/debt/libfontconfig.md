# libfontconfig technical debt

### Functional configuration lifetimes

- [FcConfigCreate, FcConfigReference and FcConfigDestroy](../../../core/libs/prx/libfontconfig/Config.cpp) implement the standard Fontconfig C signatures and explicit configuration handle lifetime. A successful create returns a distinct opaque handle with one atomic reference; `FcConfigReference(config)` returns the identical pointer and retains it, and `FcConfigDestroy(config)` drops a reference and frees the state after the last owner. Null destruction is harmless; allocation failure returns null. These semantics follow Fontconfig 2.15.0 [fccfg.c](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fccfg.c) and its [public API](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/fontconfig/fontconfig.h). Native 2.15.0 differential tests cover explicit create/reference/destroy cycles and concurrent independent handles. The opaque host storage is intentionally not Fontconfig's full configuration structure: font directories, rule sets, cache paths, font discovery and loaded/global configuration are not populated and remain unavailable to callers. `FcConfigReference(NULL)` normally initializes the global current configuration; it **throws here**, rather than falsely succeeding without parsing/loading the system configuration. On Windows, crossing the SysV guest boundary with that exception is unsupported; tests do not exercise the null-reference path there. `FcConfigGetCurrent`, `FcInit` and all other configuration APIs remain throwing stubs. No console, title or PS5 module was tested; native library conformance does not verify the PS5 guest ABI or version.

### Callable exports

- [Remaining callable exports](../../../core/libs/prx/libfontconfig/Export.cpp) - 50 of the 53 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libfontconfig.c) and the [selected export metadata](https://gist.github.com/kyon-eth/7c1e54e6851733f72dbb507fdfbf35f4/206d5ec1b74f677f61a1eda755b490537209a8e1) from two version-labelled images (SHA-256 `3694ba82b2fbff0d5d789f91938906ebc8ee45262aaaba2b333d490f7e0c24ab`). The labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

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
| `FcPatternReference` | `QZuEIDVT42s` |
| `FcObjectSetDestroy` | `WtTZKbENx4o` |
| `FcPatternGet` | `Xmsz9ffM-9g` |
| `FcPatternAddMatrix` | `Zqa5tqMhOrI` |
| `FcPatternAddString` | `bU30lh8YpkE` |
| `FcFontSetCreate` | `fJfx9D9g9W4` |
| `FcLangSetHasLang` | `gbtAblHLpQg` |
| `FcCharSetCreate` | `hNyN1seFzWY` |
| `FcLangSetCreate` | `jjz2C1StauM` |
| `FcFreeTypeCharIndex` | `l240XTbw2vk` |
| `FcFontSetAdd` | `lEIEcYhU2vE` |
| `FcPatternDestroy` | `nVkAFM4iCS4` |
| `FcCharSetAddChar` | `nXYd1oE9ZM0` |
| `FcMatrixMultiply` | `oM+8JNTrd24` |
| `FcPatternAddCharSet` | `paHcj4DoPQ4` |
| `FcDefaultSubstitute` | `sqEJZ2yMVNw` |
| `FcObjectSetBuild` | `v5-8NZmvyrA` |
| `FcPatternAddLangSet` | `vqX-PYS3MPc` |
| `FcFontSetDestroy` | `vrZNehbMgsU` |
| `FcPatternGetLangSet` | `x55q-bC-i+8` |
| `FcPatternCreate` | `xGAEbwA7FNo` |
| `FcPatternFormat` | `zqe0Dw9BCpc` |
