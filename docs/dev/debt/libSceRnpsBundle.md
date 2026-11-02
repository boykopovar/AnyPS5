# libSceRnpsBundle technical debt

### Unknown function info

- [libSceRnpsBundle named exports](../../../core/libs/prx/libSceRnpsBundle/Export.cpp) - signatures and behavior are unknown. The parameterless `int` declarations are import placeholders, not ABI prototypes, and immediately throw through `NotImplemented_nid_no_patch`. The pinned [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscernpsbundle-6bc8892b2a.jsonl) identifies the five name/NID pairs below; the pinned [module map](https://github.com/boykopovar/ps5-nid-map/blob/cc1d586fddab576a520f226da6b2632b031e6d09/data/Modules/libSceRnpsBundle.inc) assigns them to `libSceRnpsBundle.sprx`. Five inventory NIDs (`0+ee+HdR0bk`, `K9DCgmMthBQ`, `j4pgxf0zD7k`, `pXKvO8AFw8k`, `t8toeP54Mx8`) have unresolved names. The map adds `1BnaCtplAzI`, which has no inventory row. None of those six is declared.

| Name | NID |
| --- | --- |
| `sceRnpsBundleInit` | `K2u0v4WEueU` |
| `sceRnpsBundleInitEx` | `hwS49XQ5OuA` |
| `sceRnpsBundleTerm` | `QmgtzAvePXM` |
| `sceRnpsBundleVerifyAndDecrypt` | `eqd58iI3rv8` |
| `sceRnpsBundleVerifyChain` | `WgPr--ikm3U` |
