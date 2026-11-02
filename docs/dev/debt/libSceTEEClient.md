# libSceTEEClient technical debt

### Unknown function info

- [libSceTEEClient pending exports](../../../core/libs/prx/libSceTEEClient/Export.cpp) - 10 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceTEEClient.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceTEEClient/Export.cpp) - 5 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. These NIDs are absent from the pinned public 3.20 catalogue. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `H9IE7wXy3qY` |
| `Unknown` | `VaBqpbmaWpc` |
| `Unknown` | `Xn5kfzvPht4` |
| `Unknown` | `ZAJdq-IONZ4` |
| `Unknown` | `d-OLk3nvFIA` |
