# libSceMediaAppSharedStorage technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceMediaAppSharedStorage/Export.cpp) - 16 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `760upNsgHcw` |
| `Unknown` | `9O8M3-DsGXw` |
| `Unknown` | `NHgygM58yx0` |
| `Unknown` | `PhlPIVLxvSo` |
| `Unknown` | `PrP8SJS-TpQ` |
| `Unknown` | `VeM9wMswCI4` |
| `Unknown` | `bEWlk0h-kT8` |
| `Unknown` | `cJqESX9JU98` |
| `Unknown` | `dn1b39zO2YE` |
| `Unknown` | `gdBO0jjfQsg` |
| `Unknown` | `omZKNhDRL9k` |
| `Unknown` | `p3mQSqkPv6U` |
| `Unknown` | `wCM+d+kYC-w` |
| `Unknown` | `xe8itWXpOZM` |
| `Unknown` | `yJ4a7GYY9kA` |
| `Unknown` | `zgDNUrp0A+U` |
