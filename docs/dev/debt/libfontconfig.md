# libfontconfig technical debt

### Silent stubs (firmware 13.42 configuration exports)

- [FcConfigCreate, FcConfigReference and FcConfigDestroy](../../../core/libs/prx/libfontconfig/Config.cpp) follow the firmware 13.42 `system_ex/common_ex/lib/libfontconfig.sprx` disassembly reported in review (SHA-256 `74d7414445f8687c1d22a92fa3c994d2e5fa304e6699dc84dcc5459af7da8472`). Both creation and reference clear the return register, regardless of the reference argument, and destruction returns without dereferencing its argument. These exports intentionally differ from desktop Fontconfig reference-counted configuration objects. This is disassembly evidence, not measured execution on a PS5, and semantics on other firmware versions are unverified.

### FreeType character lookup

- [FreeType character lookup](../../../core/libs/prx/libfontconfig/Export.cpp): `FcFreeTypeCharIndex` follows the pinned Fontconfig `fcfreetype.c` search order: current Unicode or Microsoft Symbol charmap first, then the other; symbol characters below U+0100 also try U+F000 offset. It calls libSceFreeType's guest face exports, so only faces created by that adapter are supported. The console's Fontconfig version, call signature and behavior are unmeasured. The focused test covers Unicode and null/missing glyphs; a symbol-only font is not tested.

### Unknown function info

- [FcCharSet and FcMatrix exports](../../../core/libs/prx/libfontconfig/Export.cpp) (libfontconfig) - `FcCharSetCreate`, `FcCharSetDestroy`, `FcCharSetAddChar`, `FcCharSetMerge`, `FcCharSetSubtractCount` and `FcMatrixMultiply` follow the public [fontconfig.h](https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/2.12.6/fontconfig/fontconfig.h), [fccharset.c](https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/2.12.6/src/fccharset.c) and [fcmatrix.c](https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/2.12.6/src/fcmatrix.c). The module's fontconfig version is unknown and no console or title caller was read. Cached constant character sets are not modeled.
- [FcConfigGetCurrent and FcInitBringUptoDate](../../../core/libs/prx/libfontconfig/Export.cpp) (libfontconfig, NIDs `2M54M-SKV-k` and `Ck9k-L7rz-I`) - return what firmware 13.42's `system_ex/common_ex/lib/libfontconfig.sprx` returns, read from disassembly, not measured on a console: `FcConfigGetCurrent` is `xor %eax,%eax; ret` (no current configuration, NULL) and `FcInitBringUptoDate` is `mov $0x1,%eax; ret` (FcTrue). Neither reads arguments or state in that module.

### Functional

- [Language sets and pattern core](../../../core/libs/prx/libfontconfig/Objects.cpp) follow Fontconfig 2.15.0 except for the integer getter below, not a measured console module. The console Fontconfig version, opaque object layouts, allocation-failure behavior and compatibility with externally created objects are unverified. The guest uses the public LP64 `FcMatrix` and `FcValue` layouts; pattern and language-set handles belong to this adapter. Language names, built-in property types and Unicode 15.1 case-fold data are generated from the pinned Fontconfig submodule. Font discovery, configuration and formatting still throw.
- [Pattern collection values and filtering](../../../core/libs/prx/libfontconfig/Objects.cpp) follow the public [Fontconfig 2.15.0 value and filter contract](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fcpat.c), including [character-set retention](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fccharset.c) and [language-set copies](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fclang.c). Filter input uses the adapter object-set layout. The five export signatures, borrowed set pointers, allocation-failure paths and faulting arguments have not been verified on a console. Foreign and stale handles remain unsupported.
- [Integer getter](../../../core/libs/prx/libfontconfig/Objects.cpp) was not measured on a console; the complete binary and console double insertion were not independently checked. It follows the firmware 13.42 `libfontconfig.sprx` disassembly reported in [review #5746793316](https://github.com/boykopovar/AnyPS5/pull/14435#pullrequestreview-5746793316), SHA-256 `74d7414445f8687c1d22a92fa3c994d2e5fa304e6699dc84dcc5459af7da8472`: the type check at `0x234c` accepts only integers, unlike upstream's double conversion.
- [Pattern equality](../../../core/libs/prx/libfontconfig/Objects.cpp) follows Fontconfig 2.15.0's distinct empty patterns comparing unequal and strings comparing with Unicode case folding, including the signed 16-bit case-table offsets: a fold whose narrowed offset underflows terminates the comparison, as in `fc-case.py` and `FcStrCaseWalkerLong`. The console version's equality rules are unverified.
- [Faulting arguments](../../../core/libs/prx/libfontconfig/Objects.cpp) throw through `NotImplemented_nid_no_patch` for unsupported null arguments. `FcPatternReference(nullptr)` throws and `FcPatternDestroy(nullptr)` does nothing as host-safety choices; the firmware 13.42 disassembly in the review above dereferences both at `0x9f0` and `0x7e0` respectively. Native pointer faults are not reproduced; inaccessible non-null pointers and stale handles are not validated.
- [Font sets and object sets](../../../core/libs/prx/libfontconfig/Sets.cpp) are based on disassembly of firmware 13.42 `libfontconfig.sprx` (SHA-256 `74d7414445f8687c1d22a92fa3c994d2e5fa304e6699dc84dcc5459af7da8472`), not the stock desktop Fontconfig layout. Font-set capacity is stored in bytes, grows by sixteen pointers, and stops at 1024 entries; each insertion duplicates the caller's pattern. Object sets allocate their pointer arrays inline and preserve the original argument order with duplicated names. Host safety returns on null destroy and null add, where the firmware accesses pointers without such checks. Console runtime behavior beyond disassembly is not verified.

### Callable exports

- [FcPatternGetFTFace](../../../core/libs/prx/libfontconfig/Objects.cpp) (libfontconfig, NID `HjlABSyLzNg`) - prototype and behaviour are unverified on a console. It follows Fontconfig 2.15.0 and the other typed getters here, with the face as an opaque pointer. No export here stores a FreeType face in a pattern, so it returns `FcPatternGet`'s result or `FcResultTypeMismatch`; the matching path is untested, and a null output on that path throws as in the other getters

- [Remaining callable exports](../../../core/libs/prx/libfontconfig/Export.cpp) - 9 of the 53 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libfontconfig.c) and the [selected export metadata](https://gist.github.com/kyon-eth/7c1e54e6851733f72dbb507fdfbf35f4/206d5ec1b74f677f61a1eda755b490537209a8e1) from two version-labelled images (SHA-256 `3694ba82b2fbff0d5d789f91938906ebc8ee45262aaaba2b333d490f7e0c24ab`). The labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `FcFontList` | `7+Gq-0xFybo` |
| `FcFontSetMatch` | `8DLBHeMsS1k` |
| `FcConfigSubstitute` | `8v4Xk2b+C1k` |
| `FcInit` | `AwGzFwnaitQ` |
| `FcFontRenderPrepare` | `CvqQqWqcO-0` |
| `FcFontMatch` | `FqneT2syjGE` |
| `FcFontSort` | `MApHziT-St4` |
| `FcDefaultSubstitute` | `sqEJZ2yMVNw` |
| `FcPatternFormat` | `zqe0Dw9BCpc` |
