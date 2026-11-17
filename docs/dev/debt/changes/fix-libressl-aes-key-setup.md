# libSceLibreSsl

## add Unknown function info

- [AES_set_encrypt_key and AES_set_decrypt_key guest pointers](../../../core/libs/prx/libSceLibreSsl/src/Aes.cpp) (libSceLibreSsl) - the user key and the `AES_KEY` are copied in with `GuestCopyIn_nid_no_patch`, the schedule is expanded on the host and written with one `GuestCopyOut_nid_no_patch`; an unreadable or unwritable pointer throws `std::invalid_argument` with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured. The block and mode functions still access their buffers in place.
