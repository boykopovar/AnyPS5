# libSceVideoNativeExtEssential

## change [Native video export `iwAQhvb9BvY`](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) (libSceVideoNativeExtEssential)

- [Native video export `iwAQhvb9BvY`](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) (libSceVideoNativeExtEssential) - from firmware 13.60 `libSceVideoNativeExtEssential.sprx` (SHA-256 `9c5786aba25b889a7991be6a9d6cc0a102d8635c7c50a9885a9a31e0531a9ecc`, disassembly only), returns `0xF0000003` on null 7th argument, `0xF0000008` if type is not 2/3/4, zeroes `*output` as 64-bit (`0x79f0`/`0xef80`/`0x12070`), throws `std::invalid_argument` if unwritable, rejects invalid 16-bit profile/level pairs with `0xF0000003` for type 2 (`profile` 4/5; `level` 4/6/8/10) and type 4 (`profile` 0x42/0x4D/0x64/0x80; `level` 10-51, 111), and throws past that. Widths of arguments 2-6 stay 32-bit placeholders.
