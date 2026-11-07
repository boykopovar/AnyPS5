# libicu technical debt

### Functional

- [Callable exports](../../../core/libs/prx/libicu/Export.cpp): 1698 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no ICU functionality is implemented. No matching NIDs were found in the later module inventory. Data exports require separate layout and ownership analysis.
