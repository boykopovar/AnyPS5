# libSceEmbeddedTts technical debt

### Functional

- [Older callable NIDs](../../../core/libs/prx/libSceEmbeddedTts/Export.cpp): 8 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
