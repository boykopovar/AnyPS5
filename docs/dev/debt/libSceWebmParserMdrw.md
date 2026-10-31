# libSceWebmParserMdrw technical debt

- [Unidentified export `wVIrSj9g--M`](../../../core/libs/prx/libSceWebmParserMdrw/Export.cpp) is an import-resolution declaration that throws through `NotImplemented_nid_no_patch`. Its name, parameter list, return type and behavior are unverified; the parameterless declaration is not an ABI contract.

- [Older callable NIDs](../../../core/libs/prx/libSceWebmParserMdrw/Export.cpp): 6 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
