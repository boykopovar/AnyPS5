# libSceFont

## change [sceFontSetFontsOpenMode]

- [sceFontSetFontsOpenMode](../../../core/libs/prx/libSceFont/src/Library.cpp) (libSceFont) - stores nothing: the mode does not change how fonts are opened. It rejects a mode above 2, as the [shadPS4 port](https://github.com/shadps4-emu/shadPS4/blob/b6f0c58dba9ee75b93477703032ae4f1e0de8f1c/src/core/libraries/font/font.cpp#L5796-L5821) does. A PS5 Pro payload process (firmware 13.60) confirmed `INVALID_LIBRARY` for a null handle and showed the module faults on a non-library handle: it checks null and dereferences without validating, so answering `INVALID_LIBRARY` there is kept as a host-safety deviation. What each mode does is still unmeasured - creating a real library from a payload needs the FT module's creation parameters - and a title process was not measured. Closes when: the modes are measured on a PS5
