# libSceWebRtc technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libSceWebRtc/Export.cpp) - 5 entries are import-resolution declarations only. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `_ZN3sce2np3rtc9TerminateEv` | `-sx0eFixkek` |
| `Unknown` | `SRJUfO0JINA` |
| `Unknown` | `Z7F8YLISPDI` |
| `Unknown` | `ZZV+i+-wetM` |
| `Unknown` | `z1bMECaLQhM` |
