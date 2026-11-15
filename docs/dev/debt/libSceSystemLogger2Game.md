# libSceSystemLogger2Game technical debt

### Unknown function info

- [libSceSystemLogger2Game additional exports](../../../core/libs/prx/libSceSystemLogger2Game/Export.cpp) - these name/NID pairs are present in the 13.60 export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `sceSystemLogger2GameGetTimestamp` | `ZNlFA5YKy4A` |
| `sceSystemLogger2GameSetEvent` | `AAqtvm0YEss` |
