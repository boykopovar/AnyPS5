# libSceMouse

## add Functional

- [sceMouseClose and sceMouseRead](../../../core/libs/prx/libSceMouse/src/mouse_impl.cpp) (libSceMouse) - before sceMouseInit both return `MOUSE_ERROR_NOT_INITIALIZED` (`0x80DF0005`), ahead of the handle and count checks. A PS5 (firmware 11.60, payload process; a title process may differ) returned `0x80df0005` in one run from sceMouseClose for handles -1 and 99, and from sceMouseRead for handles -1, 99, 0 and 1 with a count of 1 and for handle 99 with counts 0, -1 and 17, leaving the buffer unchanged; the order of the checks after sceMouseInit is not measured.
