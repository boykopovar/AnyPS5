# libSceIcu

## add Functional

- [UTF-32 conversion buffers](../../../core/libs/prx/libSceIcu/Utf32.cpp) (libSceIcu) - `u_strFromUTF32WithSub` and `u_strToUTF32WithSub` (and their no-substitution forms) copy the source in once, a terminated one in page-bounded chunks, build the result on the host and write it, the length and the substitution count through checked copies. An unreadable source or unwritable output sets `U_ILLEGAL_ARGUMENT_ERROR`. The `UErrorCode` itself is still read and written in place, as everywhere in this library.
