# libSceVideoOutSecondary technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceVideoOutSecondary/Export.cpp) - Two entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Names, parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Both entries throw through `NotImplemented_nid_no_patch`. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `Unknown` | `KJWkt9y7YC4` |
| `Unknown` | `s+isXKLpwS0` |
