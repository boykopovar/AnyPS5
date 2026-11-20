# libSceLibreSsl

## change [BIO_f_ssl]

- [BIO_f_ssl](../../../core/libs/prx/libSceLibreSsl/src/Bio.cpp) - not measured on a PS5: the `ssl` name, `BIO_TYPE_SSL`, 48-byte state, allocation error, unattached free and control order follow [bio_ssl.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/bio_ssl.c#L78-L353); `long` is 64-bit. Null I/O buffers return 0 before BIO access; unattached controls return 0 except null `BIO_C_SET_SSL`, which updates shutdown then throws at the null dereference. Non-null I/O, attachment and SSL controls throw; TLS is unavailable. An unattached callback control throws at the null dereference; an attached one follows [ssl_callback_ctrl](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/bio_ssl.c#L464-L487) and [BIO_callback_ctrl](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/bio/bio_lib.c#L384-L410).
