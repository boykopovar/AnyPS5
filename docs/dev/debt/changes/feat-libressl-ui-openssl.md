# libSceLibreSsl

## add Functional

- [UI_OpenSSL](../../../core/libs/prx/libSceLibreSsl/src/Ui.cpp) (libSceLibreSsl) - unverified on PS5. Returns the static 56-byte `UI_METHOD` table `"OpenSSL default user interface"` following [LibreSSL 2.5.4 `ui_openssl.c`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/ui/ui_openssl.c#L155-L167) (unchanged in 3.4.1), with null `flush` and `constructPrompt` and four distinct `APS5_VABI` host callbacks (`open_console`, `write_string`, `read_string`, `close_console`). Calling any of the four callbacks throws via `NotImplemented_nid_no_patch` because console TTY prompting and `CRYPTO_LOCK_UI` are unimplemented.
