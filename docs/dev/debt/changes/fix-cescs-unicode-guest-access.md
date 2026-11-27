# libSceCesCs

## add Functional

- [Unicode scalar guest conversions](../../../core/libs/prx/libSceCesCs/Unicode.cpp) (libSceCesCs) - Unicode conversions read sources and write destinations through `CesGuest`, returning `CES_ERROR_INVALID_SRC_BUFFER` for an unreadable source and `CES_ERROR_INVALID_DST_BUFFER` for an unwritable target or length pointer while preserving caller errno.
