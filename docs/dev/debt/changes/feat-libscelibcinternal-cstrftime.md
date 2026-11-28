# libSceLibcInternal

## add Functional

- [_CStrftime](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - not checked against `libSceLibcInternal.sprx` or on a console. The C-locale time formatter delegates to libc `strftime`, matching the `_CStrxfrm` / `_CWcsxfrm` forwarding pattern. Null `time` throws; a null `format` follows libc and formats as `%c`. Closes when: compared with the console module.
