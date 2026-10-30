# libSceConnectAccountDialog technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceConnectAccountDialog/Export.cpp) - 7 entries observed in a 3.20-labelled image and a later 13.60-labelled image are import-resolution declarations only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled image. The extract SHA-256 is `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. This is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior.

| Name | NID |
| --- | --- |
| `sceConnectAccountDialogClose` | `9ShAQi6BXhg` |
| `sceConnectAccountDialogGetResult` | `T+wthkoDDFE` |
| `sceConnectAccountDialogGetStatus` | `BsY6ldG11CU` |
| `sceConnectAccountDialogInitialize` | `41VeaFH-BlE` |
| `sceConnectAccountDialogOpen` | `5dDAY+FchRM` |
| `sceConnectAccountDialogTerminate` | `pr2u71StfRo` |
| `sceConnectAccountDialogUpdateStatus` | `B24oCJO6Awg` |
