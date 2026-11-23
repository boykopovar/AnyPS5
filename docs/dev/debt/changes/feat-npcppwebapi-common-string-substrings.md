# libSceNpCppWebApi

## add Functional

- [Common::String substring operations](../../../core/libs/prx/libSceNpCppWebApi/StringSubstrings.cpp) search byte strings and replace ranges using the existing string buffer owner. The PPSA28050 module supplies both find overloads, the eight-byte `npos` value, replacement results and allocation-failure behavior. The fallback can use temporary host storage to assemble a replacement and uses the existing host string allocator for its final buffer. Guest custom allocators, other module versions and console execution are unverified. A count beyond the remaining characters is clamped. Positions and counts whose sum exceeds the guest word were not exercised in the module.
