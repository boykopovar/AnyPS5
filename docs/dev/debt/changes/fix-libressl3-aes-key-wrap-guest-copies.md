# libSceLibreSsl3

## add Functional

- [AES_wrap_key and AES_unwrap_key buffers](../../../core/libs/prx/libSceLibreSsl3/src/AesWrap.cpp) (libSceLibreSsl3) - the key schedule, IV and input are copied in once, the rounds run on host buffers, and the result is written out once through checked copies, so overlapping input and output still work. An unreadable key, IV or input, or an unwritable output, throws, as a null one does; LibreSSL faults there. A host-safety fallback, not measured on the console.
