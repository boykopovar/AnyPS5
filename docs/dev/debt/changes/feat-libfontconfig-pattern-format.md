# libfontconfig

## change [Language sets and pattern core]

- [Language sets and pattern core](../../../core/libs/prx/libfontconfig/Objects.cpp) follow Fontconfig 2.15.0 except for the integer getter below, not a measured console module. The console Fontconfig version, opaque object layouts, allocation-failure behavior and compatibility with externally created objects are unverified. The guest uses the public LP64 `FcMatrix` and `FcValue` layouts; pattern and language-set handles belong to this adapter. Language names, built-in property types and Unicode 15.1 case-fold data are generated from the pinned Fontconfig submodule. Font discovery and configuration still throw.

## add Functional

- [Pattern formatting](../../../core/libs/prx/libfontconfig/src/PatternFormat.cpp) follows pinned [Fontconfig 2.15.0 fcformat.c](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fcformat.c) and its name, character-set, language-set and string helpers. The console version, signature and formatting behavior remain unmeasured. Only adapter-owned patterns and values are supported; foreign handles and inaccessible pointers are unverified. A null format throws instead of reproducing upstream's pointer fault. The five builtins operate on the supplied pattern without configuration or matching. Range and FreeType-face values cannot be inserted by the existing pattern adapter. Allocation-failure injection is untested.

## change [Remaining callable exports]

- [Remaining callable exports](../../../core/libs/prx/libfontconfig/Export.cpp) - 3 of the 53 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libfontconfig.c) and the [selected export metadata](https://gist.github.com/kyon-eth/7c1e54e6851733f72dbb507fdfbf35f4/206d5ec1b74f677f61a1eda755b490537209a8e1) from two version-labelled images (SHA-256 `3694ba82b2fbff0d5d789f91938906ebc8ee45262aaaba2b333d490f7e0c24ab`). The labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

## remove | `FcPatternFormat` | `zqe0Dw9BCpc` |
