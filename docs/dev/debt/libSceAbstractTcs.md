# libSceAbstractTcs technical debt

### Functional

- [Older callable NIDs](../../../core/libs/prx/libSceAbstractTcs/Export.cpp): 40 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
