# ulobjmgr technical debt

### Silent stubs

- [ulobjmgr](../../../core/libs/prx/ulobjmgr/Export.cpp) registers no object: `_sceUlobjmgrRegisterObject` always hands out id 0 and `_sceUlobjmgrUnregisterObject` releases nothing, as shadPS4 does

### Unknown function info

- [_sceUlobjmgrRegisterObject](../../../core/libs/prx/ulobjmgr/Export.cpp) (ulobjmgr) - parameters taken from shadPS4 (PS4): a nonzero 64-bit object, a nonzero 32-bit kind and a 32-bit id output; the meaning of the first two and the PS5 signature are unverified. The id bound of `_sceUlobjmgrUnregisterObject` (below 0x4000) and the raw `EINVAL` (22) return also come from shadPS4; returning `EFAULT` (14) for an unwritable `id` store is a host-safety fallback as console behavior for unwritable memory has not been measured

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/ulobjmgr/Export.cpp) - 2 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `HZ9Q2c+4BU4` |
| `Unknown` | `SweJO7t3pkk` |
