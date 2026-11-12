# libSceNpSpeechToText technical debt

### Silent stubs

- [sceNpSpeechToTextInitialize](../../../core/libs/prx/libSceNpSpeechToText/Export.cpp) (libSceNpSpeechToText) - returns success without setting the byte at `0x14068` to 1, so the other six exports keep answering `0x8193F000` after initialization instead of proceeding as firmware does: AnyPS5 has no speech-to-text backend and no PSN connection. The store of 1 at `0x530`, the error's name and every argument layout are read from disassembly of firmware 13.42's `libSceNpSpeechToText.sprx` (SHA-256 `045bea809cf22f946ad7e8a816c43eac73a1a93e3c588ff744915512eb459a88`) and not run on a console. Closes when: measured on a PS5

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNpSpeechToText/Export.cpp) - 1 entry observed in a 3.20-labelled image and a later 13.60-labelled image is an import-resolution declaration only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. It throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled image. The extract SHA-256 is `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. This is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior.

| Name | NID |
| --- | --- |
| `sceNpSpeechToTextInitialize` | `WuUZSVsrAUM` |

- [The uninitialized answers](../../../core/libs/prx/libSceNpSpeechToText/Export.cpp) (libSceNpSpeechToText) - `sceNpSpeechToTextCreateContext`, `sceNpSpeechToTextDestroyContext`, `sceNpSpeechToTextGetEvent`, `sceNpSpeechToTextStart`, `sceNpSpeechToTextStop` and `sceNpSpeechToTextTerminate` return `0x8193F000`, only the 32-bit result being claimed, because the byte at `0x14068` that they test is never set to 1 here. In firmware 13.42's `libSceNpSpeechToText.sprx` (SHA-256 `045bea809cf22f946ad7e8a816c43eac73a1a93e3c588ff744915512eb459a88`), read from disassembly and not run on a console, each tests the byte at `0x14068` before any argument is read or any call is made and returns `0x8193F000` unless it is 1. That byte lies beyond its segment's file data, so it starts zero, and the only store of 1 to it in the executable segment is at `0x530` in `sceNpSpeechToTextInitialize`. The error's name is unverified.
