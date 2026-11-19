# libSceLibreSsl3

## remove `SSL_get0_verified_chain`

## add Functional

- [SSL_get0_verified_chain](../../../core/libs/prx/libSceLibreSsl3/src/SslVerifiedChain.cpp) (libSceLibreSsl3) - returns the peer's verified X.509 certificate stack from the SSL layer 3 handshake state (`ssl->s3->hs.verifiedChain`) following LibreSSL 4.0.0 `ssl/ssl_lib.c`; returns `nullptr` when `ssl` or `ssl->s3` is null or when no verified chain is present. The returned stack pointer is borrowed and does not increment certificate reference counts. PS5 hardware and console behavior remain unverified. Closes when: compared against console behavior.
