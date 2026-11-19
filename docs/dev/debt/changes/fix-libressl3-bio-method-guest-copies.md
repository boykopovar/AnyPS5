# libSceLibreSsl3

## add Functional

- [BIO_meth_set_* callbacks](../../../core/libs/prx/libSceLibreSsl3/src/BioMethod.cpp) (libSceLibreSsl3) - the write, read, ctrl, create and destroy setters store the callback into the caller's method record through a checked copy, keeping `errno`; an unwritable record throws, as a null one does; LibreSSL faults there. A host-safety fallback, not measured on the console.
