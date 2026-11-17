# libSceLibreSsl3

## add Functional

- [ERR_print_errors_fp](../../../core/libs/prx/libSceLibreSsl3/src/ErrorPrinting.cpp) (libSceLibreSsl3) - drains the shared libSceLibreSsl error queue with `ERR_get_error` and formats each code with `ERR_error_string_n`, writing LibreSSL's `thread:error:file:line:data` shape as `0:<error>::0:` through a guest `FileStream`; a null stream throws. The parent queue has no `ERR_get_error_line_data`, so the thread id, file, line and data fields are always empty here, and the console and the text formatting are unverified. Closes when: compared with the console.
