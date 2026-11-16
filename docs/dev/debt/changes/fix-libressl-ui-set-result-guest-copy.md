# libSceLibreSsl

## add Functional

- [UI_set_result result](../../../core/libs/prx/libSceLibreSsl/src/Ui.cpp) (libSceLibreSsl) - the caller's result is read once through checked copies, in chunks that never cross a 4 KiB boundary, and the result buffer is written once through a checked copy: the terminated result for a prompt or verify string, the answer byte for a boolean. An unreadable result or unwritable buffer throws, as a null result does; LibreSSL faults there. A host-safety fallback, not measured on the console.
