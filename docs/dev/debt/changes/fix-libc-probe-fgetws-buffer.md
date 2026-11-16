# libc

## add Unknown function info

- [fgetws buffer](../../../core/libs/prx/libc/src/FormattingWide.cpp) (libc) - the line is read into a host string and written with its terminator in one `GuestCopyOut_nid_no_patch`, after checking the first unit with `GuestWritable_nid_no_patch` so an unmapped buffer consumes nothing; an unwritable buffer sets the stream error and `errno` 22 and returns null like a null buffer, because the console's behaviour on unmapped memory is unmeasured.
