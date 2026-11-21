# libSceLibreSsl

## add Functional

- [X509_VERIFY_PARAM_set1_host name](../../../core/libs/prx/libSceLibreSsl/src/X509Store.cpp) (libSceLibreSsl) - the host name is read through checked copies in chunks that never cross a 4 KiB boundary: up to its terminator when the length is 0, else the counted bytes, stopping at an embedded NUL, which is still refused before the list changes. The copy is taken from the host bytes. An unreadable name throws, as null verify parameters do, and keeps the list; LibreSSL faults there. A host-safety fallback, not measured on the console.
