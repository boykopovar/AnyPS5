# libSceJson

## remove [_ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE]

## remove [_ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEEPFiS7_PvES8_]

## remove [_ZN3sce4Json5Value9setParentEPKS1_]

## remove [_ZN3sce4Json5ValueC1ERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE]

## remove [_ZN3sce4Json5ValueC1ERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE]

## remove [_ZN3sce4Json5ValueC1ERKSt4listIS1_NS0_8StlAllocIS1_EEE]

## remove [_ZN3sce4Json5ValueC2ERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE]

## remove [_ZN3sce4Json5ValueC2ERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE]

## remove [_ZN3sce4Json5ValueC2ERKSt4listIS1_NS0_8StlAllocIS1_EEE]

## remove [_ZN3sce4Json6Parser10parseArrayERNS0_5ValueERNS0_11InputStreamEPS2_]

## remove [_ZN3sce4Json6Parser10parseValueERNS0_5ValueERNS0_11InputStreamEPS2_]

## remove [_ZN3sce4Json6Parser11parseNumberERNS0_5ValueERNS0_11InputStreamEPS2_]

## remove [_ZN3sce4Json6Parser11parseObjectERNS0_5ValueERNS0_11InputStreamEPS2_]

## remove [_ZN3sce4Json6Parser11parseStringERNS0_5ValueERNS0_11InputStreamEPS2_]

## remove [_ZN3sce4Json6Parser11parseStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEERNS0_11InputStreamE]

## remove [_ZN3sce4Json6Parser12parseQuadHexERNS0_11InputStreamE]

## remove [_ZN3sce4Json6Parser14parseCodePointERSbIcSt11char_traitsIcENS0_8StlAllocIcEEERNS0_11InputStreamE]

## remove [_ZN3sce4Json9RootParamC1Ev]

## remove [_ZN3sce4Json9RootParamC2Ev]

## remove [_ZN3sce4Json9RootParamD1Ev]

## remove [_ZN3sce4Json9RootParamD2Ev]

## remove [_ZNK3sce4Json5Value7getRootEv]

## remove [_ZNK3sce4Json5Value8getValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE]

## remove [_ZNK3sce4Json5Value8toStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE]

## remove [_ZNK3sce4Json5ValueixERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE]

## remove The other 5 pending exports

## add Unknown function info

- [sce::Json::Value::toString and serialize real numbers](../../../core/libs/prx/libSceJson/Export.cpp) (libSceJson) - the console formats a real with `snprintf(buffer, 256, "%f")` and cuts `.000000`; here `std::to_chars` with fixed notation and 6 digits produces the same digits without the locale. A NaN is written `nan` without a sign and infinities as `inf` and `-inf`; the console's spelling of non-finite values is unverified.
- [sce::Json::Parser::parseNumber](../../../core/libs/prx/libSceJson/Export.cpp) (libSceJson) - the console converts real numbers with `atof`; here `std::strtod` is used with the decimal point adapted when the host numeric locale differs from `.`, and `errno` is restored. Whether the console's `atof` differs on `errno` or underflow handling is unverified.

## add Functional

- [sce::Json::Malloc and allocation failures](../../../core/libs/prx/libSceJson/Export.cpp) (libSceJson) - when the title's `MemAllocator` returns null the console throws `int` -1 from its own frames; here a host `int` exception is thrown, which reaches the title only through the host unwinder. Where the console dereferences a null pointer (a container allocated before `Initializer::initialize`, the allocator returning null inside `initialize`, a string, array or object value whose container was never allocated) a `std::runtime_error` is thrown instead.
- [sce::Json::Parser::parse(Value&, const char*, size_t) with an unterminated string](../../../core/libs/prx/libSceJson/Export.cpp) (libSceJson) - past the end of the buffer the console's reader returns 0 without writing, so the string grows with `0xff` bytes until the allocator fails and parse returns 0x80848102 with the value cleared. Here reaching the end of the buffer inside a string returns 0x80848102 and clears the value at once; with an allocator that never fails the console would not return.
