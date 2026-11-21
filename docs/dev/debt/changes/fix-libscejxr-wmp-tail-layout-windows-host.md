# libSceJxr

## add Functional

- [WMP record tails on hosts with a 4-byte long](../../../core/libs/prx/libSceJxr/src/NativeWmp.c) (libSceJxr) - not measured on a PS5. jxrlib's `Long` is 4 bytes on a Windows host, so the fields from `WMP.bHasAlpha` on of [PKImageEncode](https://github.com/4creators/jxrlib/blob/f7521879862b9085318e814c6157490dd9dbbdb4/jxrgluelib/JXRGlue.h#L452-L463) and [PKImageDecode](https://github.com/4creators/jxrlib/blob/f7521879862b9085318e814c6157490dd9dbbdb4/jxrgluelib/JXRGlue.h#L541-L557) sit elsewhere than in the LP64 guest record. The record keeps the LP64 layout and is converted to the host one only around the jxrlib calls that use those fields. The four `Long` offsets and sizes keep 32 bits there, and a stream callback that throws through jxrlib leaves the record in the host layout. Closes when: the console layout is measured.
