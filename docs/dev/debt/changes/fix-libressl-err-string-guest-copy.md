# libSceLibreSsl

## add Unknown function info

- [ERR_error_string_n buffer](../../../core/libs/prx/libSceLibreSsl/ErrorQueue.cpp) (libSceLibreSsl) - the string is formatted on the host and written with one `GuestCopyOut_nid_no_patch` of its length plus the terminator; an unwritable buffer throws `std::invalid_argument` with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured.
