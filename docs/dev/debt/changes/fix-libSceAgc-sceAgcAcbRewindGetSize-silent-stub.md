# libSceAgc

## add Silent stubs

- [sceAgcAcbRewindGetSize](../../../core/libs/prx/libSceAgc/Acb/src/Control.cpp) (libSceAgc) - returns 8 dwords, the rewind command-packet size estimate; a PS5 (firmware 11.60, payload process; a title process may differ) returned 8 in three calls of one run, also with the argument registers 0, 1, 2, 3, and no signal (the `int (void)` prototype and the unit of dwords are assumed), so the return value is measured; the size of a packet that is written was not measured.
