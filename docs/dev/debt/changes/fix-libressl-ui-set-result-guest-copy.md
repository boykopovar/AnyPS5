# libSceLibreSsl

## add Functional

- [UI_set_result result](../../../core/libs/prx/libSceLibreSsl/src/Ui.cpp) (libSceLibreSsl) - the caller's result is read once through checked copies, in chunks that never cross a 4 KiB boundary, and the result buffer is written once through a checked copy: the terminated result for a prompt or verify string, the answer byte for a boolean. An unreadable result or unwritable buffer throws, as a null result does; LibreSSL faults there. A host-safety fallback, not measured on the console.
- [UI method records](../../../core/libs/prx/libSceLibreSsl/src/Ui.cpp) (libSceLibreSsl) - `UI_create_method` reads the name once through checked copies in page-bounded chunks before allocating the record; `UI_destroy_method` and the `UI_method_set_*`/`UI_method_get_*` opener, writer, reader and closer calls read and write the record's fields through checked copies. An unreadable record gets the null-record result (-1, null, or a throw from `UI_destroy_method`) and an unreadable name throws; LibreSSL faults there. A host-safety fallback, not measured on the console.
