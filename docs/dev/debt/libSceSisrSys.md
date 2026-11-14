# libSceSisrSys technical debt

### Unknown function info

- [sceSisrSysProcessUmdEvent and sceSisrSysSetup](../../../core/libs/prx/libSceSisrSys/Export.cpp) - names and NIDs are present in the complete [PS5 13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscesisrsys-d8069f882d.jsonl), and the pinned [module map](https://github.com/boykopovar/ps5-nid-map/blob/cc1d586fddab576a520f226da6b2632b031e6d09/data/Modules/libSceSisrSys.inc) assigns both identities to the physical `libSceSisrSys.sprx` provider. Parameter lists, return types and behavior are unverified; the parameterless `int` declarations are import-resolution placeholders, not verified ABI contracts. Both entries throw through `NotImplemented_nid_no_patch`. No SISR setup or UMD-event behavior is implemented.
