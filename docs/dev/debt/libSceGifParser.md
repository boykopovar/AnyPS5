# libSceGifParser technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libSceGifParser/Export.cpp) - Seven 3.20 callable NIDs are import-resolution declarations only. One NID also appears in the 13.60-labelled inventory; other NIDs differ across versions. Their signatures and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented. Data exports require separate layout and ownership analysis.
