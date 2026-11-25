# libSceLibreSsl3

## add Functional

- [ObjectFromText](../../../core/libs/prx/libSceLibreSsl3/src/ObjectFromText.cpp) - `EncodeText` and `DecimalArc` now match firmware 13.60 `libSceLibreSsl3.sprx` (`OBJ_txt2obj` -> `a2d_ASN1_OBJECT`): reject `first < 2` with `second >= 40`, reject leading whitespace on the first arc, reject `"00"` double-zero arc prefixes, and reject invalid/zero-NID entries in `NamedObject`.
