# libSceLibreSsl3

## add Unknown function info

- [i2d_PKCS8_PRIV_KEY_INFO output](../../../core/libs/prx/libSceLibreSsl3/src/KeyDer.cpp) (libSceLibreSsl3) - the guest `unsigned char** output` is read once with `GuestCopyIn_nid_no_patch`, the encoding is written to a caller buffer with one `GuestCopyOut_nid_no_patch`, and the advanced or newly allocated pointer is written back with one checked copy; an unreadable or unwritable pointer or buffer throws an invalid-argument exception, with `errno` preserved and a block allocated by the call freed, because the console's behaviour on unmapped memory is unmeasured. The `PKCS8_PRIV_KEY_INFO` itself is still read in place.
