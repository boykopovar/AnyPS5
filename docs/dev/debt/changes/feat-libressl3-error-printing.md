# libSceLibreSsl3

## add Functional

- [ERR_print_errors_fp](../../../core/libs/prx/libSceLibreSsl3/src/ErrorPrinting.cpp) (libSceLibreSsl3) - writes each queued error through `ERR_print_errors_cb` to a guest `FileStream` with `fwrite`, as LibreSSL `err_prn.c` does, so each line matches the callback output (`<thread>:<error>:<file>:<line>:`); a null stream throws. The console text formatting is unverified. Closes when: compared with the console.
