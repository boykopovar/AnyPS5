# libSceLibreSsl

## add Unknown function info

- [CMAC_CTX_new, CMAC_CTX_free, CMAC_Init, CMAC_Update, CMAC_Final](../../../core/libs/prx/libSceLibreSsl/src/Cmac.cpp) (libSceLibreSsl) - unreadable or unwritable guest `CMAC_CTX`, `EVP_CIPHER`, key, data, cipher data, output tag, and output length pointers throw `std::invalid_argument` after `GuestArenaPrepareHostRead` / `GuestArena::HostWrite` validation instead of dereferencing guest memory directly; console fault behavior for invalid guest pointers is not measured.
