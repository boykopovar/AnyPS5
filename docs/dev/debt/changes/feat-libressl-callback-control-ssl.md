# libSceLibreSsl

## change [BIO_f_ssl]

- [BIO_f_ssl](../../../core/libs/prx/libSceLibreSsl/src/Bio.cpp) - partly measured on a PS5 (fw 11.60): `BIO_TYPE_SSL` (0x207), name `ssl`, 64-bit `long` ([bio_ssl.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/bio_ssl.c)). `readSsl`, `writeSsl`, `destroySsl`, `callbackControlSsl` and `controlSsl` (`SET_SSL`, `GET_SSL`, `DO_STATE_MACHINE`, `FLUSH`, `PENDING`/`WPENDING`, `INFO`, `GET_CLOSE`/`SET_CLOSE`, `GET_SSL_NUM_RENEGOTIATES`) follow 2.5.4. Null `SET_SSL` sets shutdown then throws; re-attaching over an SSL or `next_bio`, renegotiation controls, pending `S3I->renegotiate` or active triggers, `RESET`, `SSL_MODE`, `PUSH`/`POP`/`DUP`, `GET_FD`, `SET`/`GET_CALLBACK` and unknown controls throw; `FLUSH` and `WANT_CONNECT` throw if `next_bio` is null; copy failures throw. `WANT_X509_LOOKUP`/`ACCEPT`/`CONNECT` retry paths are untested (the engine never returns them).
