# libMozaSDK technical debt

### Functional

- [Exports](../../../core/libs/prx/libMozaSDK/Export.cpp): the library's exports, signatures and behavior are unknown, so it exports only a throwing import-resolution placeholder. The library is absent from the PS5 3.20 export inventory that the other placeholder libraries come from, and its firmware source and version are not verified. PPSA01317 (01.710.000) lists `libMozaSDK.prx` as a needed module and library without shipping it and binds no import to it, so on Linux the loader stops before the title runs without a file of that name; Windows skips the missing dependency.
