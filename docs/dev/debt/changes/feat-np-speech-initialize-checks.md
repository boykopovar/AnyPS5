# libSceNpSpeechToText

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceNpSpeechToText/Export.cpp) - 1 entry observed in a 3.20-labelled image and a later 13.60-labelled image. In firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21864#issuecomment-6942453368), disassembly only, not run on a console) `sceNpSpeechToTextInitialize(const void* p)` returns `0x8193F003` unless `p` is non-null, its 32-bit `p+0x00` is `0x20000` and every byte from `p+0x04` to `p+0x3f` is 0; here an unreadable `p` gives the same code, a host-safety fallback. A valid `p` throws: the console then only sets up module state, but the other exports' initialized paths weren't read. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

# libSceNpTextToSpeech

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceNpTextToSpeech/Export.cpp) - 1 entry observed in a 3.20-labelled image and a later 13.60-labelled image. In firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21864#issuecomment-6942453368), disassembly only, not run on a console) `sceNpTextToSpeechInitialize(const void* p)` returns `0x8194F004` unless `p` is non-null, its 32-bit `p+0x00` is `0x20000`; here an unreadable `p` gives the same code, a host-safety fallback. A valid `p` throws: the console then only sets up module state, but the other exports' initialized paths weren't read. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).
