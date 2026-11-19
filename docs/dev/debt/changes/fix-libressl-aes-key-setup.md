# libSceLibreSsl

## add Unknown function info

- [AES_set_encrypt_key and AES_set_decrypt_key guest pointers](../../../core/libs/prx/libSceLibreSsl/src/Aes.cpp) (libSceLibreSsl) - the user key and the `AES_KEY` are copied in with `GuestCopyIn_nid_no_patch`, the schedule is expanded on the host and written with one `GuestCopyOut_nid_no_patch`; an unreadable or unwritable pointer throws `std::invalid_argument` with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured. `AES_cbc_encrypt`, `AES_cfb128_encrypt` and `AES_ctr128_encrypt` copy the key in once and stream their input and output through 4 KiB host chunks; the IV, counter, keystream and position are copied in once and written back only when the call changes them, with the same exception. `AES_encrypt`, `AES_decrypt` and `AES_ecb_encrypt` still access their key and block in place.
