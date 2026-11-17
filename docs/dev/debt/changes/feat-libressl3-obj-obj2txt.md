# libSceLibreSsl3

## add Functional

- [OBJ_obj2txt](../../../core/libs/prx/libSceLibreSsl3/src/ObjObj2Txt.cpp) - prints a name when `no_name` is clear: the object's long name, then its short name, then the short name of the NID its encoding resolves to, so a caller-built object with no names still prints `rsaEncryption`. LibreSSL resolves the NID first and prints its long name, so a name taken from the NID here is the short name, because no long-name accessor is exported. Otherwise it decodes the base-128 arcs as `OBJ_obj2txt` does and returns 0 for a trailing continuation byte; a null object or data returns 0 with `buffer[0]` cleared. Console behaviour is unverified. Closes when: compared with the console.

## remove `xZ1BME5qDv0`
