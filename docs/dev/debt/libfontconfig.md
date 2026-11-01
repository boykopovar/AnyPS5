# libfontconfig technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libfontconfig/Export.cpp) - 32 remaining callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libfontconfig.c) and the [selected export metadata](https://gist.github.com/kyon-eth/7c1e54e6851733f72dbb507fdfbf35f4/206d5ec1b74f677f61a1eda755b490537209a8e1) from two version-labelled images (SHA-256 `3694ba82b2fbff0d5d789f91938906ebc8ee45262aaaba2b333d490f7e0c24ab`). The labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `FcConfigGetCurrent` | `2M54M-SKV-k` |
| `FcFontList` | `7+Gq-0xFybo` |
| `FcFontSetMatch` | `8DLBHeMsS1k` |
| `FcConfigSubstitute` | `8v4Xk2b+C1k` |
| `FcInit` | `AwGzFwnaitQ` |
| `FcInitBringUptoDate` | `Ck9k-L7rz-I` |
| `FcFontRenderPrepare` | `CvqQqWqcO-0` |
| `FcFontMatch` | `FqneT2syjGE` |
| `FcPatternFilter` | `H9TOtqlqmHM` |
| `FcConfigCreate` | `HGtDYp4ddNE` |
| `FcPatternGetFTFace` | `HjlABSyLzNg` |
| `FcCharSetSubtractCount` | `J8GcxU03WkE` |
| `FcFontSort` | `MApHziT-St4` |
| `FcCharSetMerge` | `MafDsPXgY6k` |
| `FcPatternGetCharSet` | `NKrjlO4HBhg` |
| `FcCharSetDestroy` | `Nw0ilSm8FxY` |
| `FcConfigDestroy` | `Oor7ukCXZOE` |
| `FcObjectSetDestroy` | `WtTZKbENx4o` |
| `FcFontSetCreate` | `fJfx9D9g9W4` |
| `FcCharSetCreate` | `hNyN1seFzWY` |
| `FcFreeTypeCharIndex` | `l240XTbw2vk` |
| `FcFontSetAdd` | `lEIEcYhU2vE` |
| `FcCharSetAddChar` | `nXYd1oE9ZM0` |
| `FcMatrixMultiply` | `oM+8JNTrd24` |
| `FcPatternAddCharSet` | `paHcj4DoPQ4` |
| `FcDefaultSubstitute` | `sqEJZ2yMVNw` |
| `FcObjectSetBuild` | `v5-8NZmvyrA` |
| `FcPatternAddLangSet` | `vqX-PYS3MPc` |
| `FcFontSetDestroy` | `vrZNehbMgsU` |
| `FcPatternGetLangSet` | `x55q-bC-i+8` |
| `FcConfigReference` | `yQed09jA3VA` |
| `FcPatternFormat` | `zqe0Dw9BCpc` |

### Functional

- [Language sets and pattern core](../../../core/libs/prx/libfontconfig/Objects.cpp) follow Fontconfig 2.15.0, not a measured console module. The console Fontconfig version, opaque object layouts, allocation-failure behavior and compatibility with externally created objects are unverified. The guest uses the public LP64 `FcMatrix` and `FcValue` layouts; pattern and language-set handles belong to this adapter. Language names, built-in property types and Unicode 15.1 case-fold data are generated from the pinned Fontconfig submodule. Font discovery, configuration, charset-valued and language-set-valued patterns, formatting and filtering still throw.
- [Pattern equality](../../../core/libs/prx/libfontconfig/Objects.cpp) follows Fontconfig 2.15.0's distinct empty patterns comparing unequal and strings comparing with Unicode case folding, including the signed 16-bit case-table offsets: a fold whose narrowed offset underflows terminates the comparison, as in `fc-case.py` and `FcStrCaseWalkerLong`. The console version's equality rules are unverified.
- [Faulting arguments and integer conversion](../../../core/libs/prx/libfontconfig/Objects.cpp) throw through `NotImplemented_nid_no_patch` for null arguments that the public implementation dereferences and for non-finite or out-of-range double-to-integer conversions. Native pointer faults and undefined C conversions are not reproduced; inaccessible non-null pointers and stale handles are not validated.
