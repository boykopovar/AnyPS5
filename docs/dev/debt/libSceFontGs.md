# libSceFontGs technical debt

### Unknown function info

- [`sceFontGraphicsGnmSurfaceInit`, `sceFontSelectGraphicsGnm` and four unnamed exports](../../../core/libs/prx/libSceFontGs/Export.cpp) (libSceFontGs) - parameter lists, return types and behaviour are unverified, so every entry throws through `NotImplemented_nid_no_patch`; the parameterless `int` declarations only resolve imports. The [PS5 13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/226383a7fa4b1275d6cebf61eb1c1e7ca7b58bcb/data/firmware-13.60/libscefontgs-b42f55c1c1.jsonl) names two of them and lists `DI4LrTUrD04`, `NtbMKwRg6qg`, `UsBLoyXsQZs` and `uksHKVEsxu4` without names; the pinned [module map](https://github.com/boykopovar/ps5-nid-map/blob/cc1d586fddab576a520f226da6b2632b031e6d09/data/Modules/libSceFontGs.inc) assigns all six to `libSceFontGs`. The six exports shared with `libSceFontGs.native` are left to that library.
