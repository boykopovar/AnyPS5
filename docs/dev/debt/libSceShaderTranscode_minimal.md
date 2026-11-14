# libSceShaderTranscode_minimal technical debt

### Functional

- [Exports](../../../core/libs/prx/libSceShaderTranscode_minimal/Export.cpp): the library's exports, signatures and behavior are unknown, so it exports only a throwing import-resolution placeholder. Final Fantasy Resonance (PPSA31544, 01.000.001) lists `libSceShaderTranscode_minimal.prx` as a needed library of `eboot.bin` and of its bundled `fakelib/libSceAgc.sprx` without shipping it and binds no import to it, so on Linux the loader stops before the title runs without a file of that name; Windows skips the missing dependency.
