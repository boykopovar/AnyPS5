# libSceImageUtil technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceImageUtil/Export.cpp) - 5 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `CttPwxlzc1A` |
| `Unknown` | `o9hdefxVx-0` |
| `Unknown` | `pjWv++MzaJY` |
| `Unknown` | `rSoaNeazErw` |
| `Unknown` | `yjU-5r3TDtc` |

- [Later callable exports](../../../core/libs/prx/libSceImageUtil/Export.cpp) - the function exports of `libSceImageUtil` listed below throw through `NotImplemented_nid_no_patch` on every call; their names, parameter lists, return types and behavior are unknown, and the parameterless `int` declarations only resolve imports. The [PS5 13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/ba1933f305f1934ed2a12c94479a977844bdbe5a/data/firmware-13.60/libsceimageutil-196f49c23b.jsonl) lists them as function exports of the 13.60-labelled image only, not of its 3.20 reference; the label does not authenticate retail firmware. `tools/nid_names.py` resolves none of them, so each uses a literal NID alias.

| Name | NID |
| --- | --- |
| `Unknown` | `J805kfu8t4w` |
| `Unknown` | `PaJr2GfStvA` |
| `Unknown` | `TBLqZYmwmnU` |
| `Unknown` | `WePG8ETDGLg` |
| `Unknown` | `dBpqY1nOvJQ` |
| `Unknown` | `eMlkZeoHuxM` |
| `Unknown` | `oOjUpbj5GyI` |
| `Unknown` | `q-Jp4vQAAXM` |

- [Older callable NIDs](../../../core/libs/prx/libSceImageUtil/Export.cpp): 5 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
