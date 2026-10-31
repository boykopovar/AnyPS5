# libSceAvcap2mvr technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAvcap2mvr/Export.cpp) - 13 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled image. The extract SHA-256 is `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. This is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior.

| Name | NID |
| --- | --- |
| `Unknown` | `AVuH0FujF30` |
| `Unknown` | `JgAC1LkbWns` |
| `Unknown` | `Kt2IOGEfaFc` |
| `Unknown` | `Mq1MuGlr48g` |
| `Unknown` | `QSLdUawhEhg` |
| `Unknown` | `R5tFB0tMbgY` |
| `Unknown` | `Tjmx+DOF65c` |
| `Unknown` | `ZMR+Q0NYcmU` |
| `Unknown` | `nPlusHmt9NY` |
| `Unknown` | `s7aQGkLitBU` |
| `Unknown` | `sM27f1nWpCc` |
| `Unknown` | `tq-PG8QaNDM` |
| `Unknown` | `u4wtg4AegL8` |
