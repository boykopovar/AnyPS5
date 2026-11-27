# libfreetype

## change [Older callable NIDs]

- [Older callable NIDs](../../../core/libs/prx/libfreetype/Export.cpp): The [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) names these exports `FTC_Manager_New`, `FT_Stream_OpenBzip2`, `FT_Stream_OpenGzip` and `FT_Stream_OpenLZW`, and the NID of each name matches. The cache manager constructor and compressed-stream helpers follow bundled FreeType. Console signatures and behavior are unverified. No matching NIDs were found in the later module inventory.

## add Functional

- [FTC_Manager_New](../../../core/libs/prx/libfreetype/Export.cpp): The console signature, requester ABI and behavior remain unverified. The adapter follows [FreeType 2.15.0](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/cache/ftcmanag.c#L336-L432) and accepts a live library from `libSceFreeType`. Its requester must return a live guest face from that library. Guest cache lookup and explicit manager destruction are unavailable. Native managers are destroyed before their guest library closes. A byte limit beyond the host `FT_ULong` range throws on Windows. Synthetic tests exercise guest callbacks, cache hits, eviction and shutdown through the native lookup API, and check the constructor through both export forms.
