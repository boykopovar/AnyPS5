# libSceLibreSsl

## change [SSL_get1_session](../../../core/libs/prx/libSceLibreSsl/Export.cpp#L528)

- [SSL_get1_session](../../../core/libs/prx/libSceLibreSsl/src/SslSession.cpp) not measured on a PS5: its signature follows [LibreSSL 2.5.4 `ssl.h`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl.h#L1433), and reference acquisition follows [`ssl_sess.c`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_sess.c#L158-L175), the version reported for firmware 13.40. The reference count sits at offset 160 of the session, after the fields of the [`ssl_session_st` layout](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl.h#L435-L478) on LP64. A checked atomic increment stands in for the `CRYPTO_LOCK_SSL_SESSION` lock: a count already at `INT_MAX` throws and stays unchanged, where LibreSSL's `references++` overflows. A null connection throws where LibreSSL dereferences it; an empty session returns null.

## change [SSL_SESSION_free](../../../core/libs/prx/libSceLibreSsl/Export.cpp)

- [SSL_SESSION_free](../../../core/libs/prx/libSceLibreSsl/src/SslSession.cpp) - releasing the last reference is unimplemented and not measured on a PS5: it throws and leaves the count at 1, because the session's internal state, peer certificate and cipher stack are not mapped. A null session returns at once, a count of 0 or less throws and stays unchanged, where LibreSSL would decrement it again, and releasing any other reference decrements the count, following LibreSSL 2.5.4 [`ssl_sess.c`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_sess.c#L680-L713).
