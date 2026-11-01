# libSceNKWebKitRequirements technical debt

### Known export names; ABI prototypes unverified

- 123 pending exports in [Export.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.
- The 112 of them declared in [Identified.hpp](../../../core/libs/prx/libSceNKWebKitRequirements/include/Identified.hpp) carry the libjpeg, libpsl, SQLite, libxml2 and UndefinedBehaviorSanitizer names whose `tools/nid_names.py` hash equals the inventory NID; the names were taken from the identifiers of the libjpeg-turbo 3.0.4, libpsl 0.21.5, SQLite 3.46.1 and libxml2 2.13.4 sources and from the two `__ubsan_handle_cfi_check_fail` handlers libkernel_sys exports. A name match does not show that the console function has that library's prototype or behavior. The other 11 hash to no identifier of those sources or of zlib 1.3.1, ICU 76.1, HarfBuzz 10.0.1, cairo 1.18.2, pixman 0.42.2, FreeType 2.13.3, libpng 1.6.44, Expat 2.6.3, libxslt 1.1.42, libwebp 1.4.0, Brotli 1.1.0, WOFF2 1.0.2, curl 8.10.1, Little CMS 2.16, OpenJPEG 2.5.2 and Fontconfig 2.15.0.

### Functional

- [zlib exports](../../../core/libs/prx/libSceNKWebKitRequirements/Zlib.cpp) - a valid gzip or automatic-detection `windowBits` throws: the vendored zlib 1.3.2 of libjpeg-turbo is built with `NO_GZIP`. The [zlib.h](https://github.com/madler/zlib/blob/51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf/zlib.h) prototypes and the LP64 112-byte `z_stream` are assumed because the NIDs of `deflateInit2_`, `deflate`, `deflateReset`, `deflateEnd`, `deflateBound`, `inflateInit2_`, `inflate`, `inflateReset`, `inflateEnd` and `uncompress` hash to those names; neither is measured on a console. The console's zlib version is unknown, so `deflateBound` values and compressed bytes follow 1.3.2 and may differ (zlib 1.3 bounds 1000 bytes on a null stream at 1139, 1.3.2 at 1151). The stream's host bookkeeping is not allocated through the guest `zalloc`. On Windows, where the host `uLong` is 32-bit, a call clears a guest-set upper half of `adler` even where zlib leaves the field alone. `uncompress` with a null length throws.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKitRequirements/Export.cpp) - 32 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. These NIDs are absent from the pinned public 3.20 catalogue. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `++QInfMOBN0` |
| `Unknown` | `+oA7tlz2Aiw` |
| `Unknown` | `1MRhOnElUw4` |
| `Unknown` | `1T7K-PY5paE` |
| `Unknown` | `3yG30EKj-64` |
| `Unknown` | `4dTqu8uWd6Q` |
| `Unknown` | `6-rd4IXD0mM` |
| `Unknown` | `68aoiQnJutE` |
| `Unknown` | `8u1QXzSpyno` |
| `Unknown` | `JwB900c8Nyo` |
| `Unknown` | `KCP1Db-mniM` |
| `Unknown` | `NCGIxzXT-KA` |
| `Unknown` | `TAGTeghnmJs` |
| `Unknown` | `TNO-EUJZf3c` |
| `Unknown` | `Wz6vEl9ATDQ` |
| `Unknown` | `YpZ1fAOlVoo` |
| `Unknown` | `ZSkWe-fNsuY` |
| `Unknown` | `cgNin5rMqA8` |
| `Unknown` | `cq16C8ickjY` |
| `Unknown` | `hjlKZNuv+v8` |
| `Unknown` | `o6hmqtmcw10` |
| `Unknown` | `o7ktMsqiCBU` |
| `Unknown` | `rW6yS0aJbjQ` |
| `Unknown` | `rgWcud6b43M` |
| `Unknown` | `soW4BQ5l3Os` |
| `Unknown` | `tc5h+wVQWZI` |
| `Unknown` | `tu7+XEdsUA8` |
| `Unknown` | `u4SbDPju5eo` |
| `Unknown` | `uXf4oddEz5E` |
| `Unknown` | `uiTgPO1VRaA` |
| `Unknown` | `x2xMh95HEb4` |
| `Unknown` | `z5L7zirdMWM` |
