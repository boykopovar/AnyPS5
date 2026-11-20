# libSceLibreSsl

## change [X509_NAME_print_ex]

- [X509_NAME_print_ex](../../../core/libs/prx/libSceLibreSsl/src/X509NamePrintEx.cpp) (libSceLibreSsl) - no console call is verified. Output follows the pinned [libressl-openbsd](../../../3rdparty/libressl-openbsd) submodule, not the 2.5.4 sources the rest of this module tracks: the adapter runs the submodule's `X509_NAME_print_ex` (a libcrypto subset, with `X509_NAME_print` and `ASN1_tag2str` copied into `libressl/x509_name_print.c`) into a memory BIO, then copies the result to the guest BIO. A BMPString or UniversalString holding a partial character fails before its value prints, because the pinned `do_buf` reads past such a buffer. BOOLEAN DER content remains the low byte of the stored string pointer through the same `ASN1_TYPE` union reuse. A guest-BIO write failure returns -1, or 0 with the X509 `ERR_R_BUF_LIB` error in compatibility mode, but partial output is still delivered; errors the submodule queues stay in its own queue.
