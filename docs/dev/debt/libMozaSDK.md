# libMozaSDK technical debt

### Functional

- [Exports](../../../core/libs/prx/libMozaSDK/Export.cpp): the library's exports, signatures and behavior are unknown, so it exports only a throwing import-resolution placeholder. Gran Turismo 7 (PPSA01317, 01.710.000) lists `libMozaSDK.prx` as a needed module and library without shipping it and binds no import to it, so on Linux the loader stops before the title runs without a file of that name; Windows skips the missing dependency.
