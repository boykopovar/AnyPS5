# libSceMediaFrameworkUtil technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceMediaFrameworkUtil/Export.cpp) - 5 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `Unknown` | `3i18MZVtRPw` |
| `Unknown` | `IJdieD-pMaQ` |
| `Unknown` | `jWvrCR+qXww` |
| `Unknown` | `nW1iwrnYBns` |
| `Unknown` | `nzWjvCRVgEM` |
