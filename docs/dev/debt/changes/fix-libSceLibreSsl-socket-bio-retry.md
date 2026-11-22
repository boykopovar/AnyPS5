# libSceLibreSsl

## add

- Socket BIO retry flags are unverified on PS5. The read and write callbacks in [BioFileSocket.cpp](../../../core/libs/prx/libSceLibreSsl/src/BioFileSocket.cpp) follow LibreSSL 2.5.4's pinned [bss_sock.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/bio/bss_sock.c#L131-L161) and [BIO_sock_non_fatal_error](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/bio/bss_sock.c#L212-L238): clear the retry bits after a transfer, then set retry-read or retry-write for EINTR, EAGAIN, EINPROGRESS, EALREADY or ENOTCONN. Error numbers come from the guest network errno slot, not host errno; sceNet failures are normalized to -1 at the BIO boundary. A zero-byte result must not reuse an earlier network error. This does not add transport or handshake support beyond the existing BIO adapter.
