# libSceLibreSsl3

## add Functional

- [OBJ_obj2txt](../../../core/libs/prx/libSceLibreSsl3/src/ObjObj2Txt.cpp) - prints a name when `no_name` is clear: the long name, else the short name, of the object its encoding resolves to in the LibreSSL3 object table, as LibreSSL does, so a caller-built object with no names still prints `rsaEncryption`; an encoding that is not in the table falls back to the object's own long name, then its short name, which LibreSSL never reads, and then to the dotted form. Otherwise it decodes the base-128 arcs as `OBJ_obj2txt` does and returns 0 for a trailing continuation byte; a null object or data returns 0 with `buffer[0]` cleared. Console behaviour is unverified. Closes when: compared with the console.

## remove `xZ1BME5qDv0`
