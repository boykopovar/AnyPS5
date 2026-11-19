# libpng16

## change [Older callable NIDs]

- [Older callable NIDs](../../../core/libs/prx/libpng16/Export.cpp): 177 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.

## add Functional

- [PNG integer readers](../../../core/libs/prx/libpng16/IntegerReaders.cpp) implement only `png_get_uint_16` and `png_get_uint_32`. Their names hash to the NIDs in the [PS5 3.20 import catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libpng16.c#L697-L698). For readable input buffers the ABI and big-endian conversion follow the [bundled libpng header](https://github.com/pnggroup/libpng/blob/cd952f49f95bb27154ae77dbb103032d95f6e580/png.h#L2784-L2791). Console behavior has not been measured. The other exports and full PNG read/write support through this module remain unimplemented.
