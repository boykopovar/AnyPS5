# libSceAgc

## add Silent stubs

- [sceAgcAcbJumpGetSize](../../../core/libs/prx/libSceAgc/Acb/src/Control.cpp) (libSceAgc) - returns 16 dwords, the jump command-packet size estimate; a PS5 (firmware 11.60, payload process; a title process may differ) returned 16 in three calls of one run, also with the argument registers 0, 1, 2, 3, and no signal (the `int (void)` prototype and the unit of dwords are assumed), so the return value is measured; the size of a packet that is written was not measured.
