# libSceNKWeb technical debt

### Functional

- [sceNKWebInitialize and sceNKWebTerminate](../../../core/libs/prx/libSceNKWeb/Export.cpp) remain throwing stubs. Both native exports take no input arguments and return a 32-bit status; decompiler SIMD parameters are unused register propagation through initialization and cleanup helpers.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWeb/Export.cpp) - 3 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `HTKKQjsQMiQ` |
| `Unknown` | `HvqTa7FlgMI` |
| `Unknown` | `q+Emt1gYris` |
