# libSceXml

## change [Util::strResult]

- [Util::strResult](../../../core/libs/prx/libSceXml/Export.cpp) (libSceXml) - returns the name firmware 13.42's `libSceXml.sprx` gives each of its 42 result codes and 0, spelling included, and `Unknown error.` for every other value: read from disassembly to every return, and measured on a PS5 (payload process, firmware 11.60, one run): the 43 texts, spelling included, and `Unknown error.` for seven other values (0xffffffff, 1, 0x7fffffff, 0x80000000, 0x00850004, 0x80850300 and 0x00860000) match; a title process was not measured, the prototype `const char* strResult(int)` is assumed, and the returned addresses were not compared. The strings are this library's own copies, so their addresses differ from the module's.
