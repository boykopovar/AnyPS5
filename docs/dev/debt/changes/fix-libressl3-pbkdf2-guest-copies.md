# libSceLibreSsl3

## add Functional

- [PKCS5_PBKDF2_HMAC guest buffers](../../../core/libs/prx/libSceLibreSsl3/src/Pbkdf2.cpp) (libSceLibreSsl3) - the password and salt are copied in once and each derived block is written out once through checked copies. An unreadable password or salt, or an unwritable output, throws, where LibreSSL faults; this is a host-safety fallback, not measured on the console.
