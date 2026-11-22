# libSceLibreSsl

## change [File and socket BIO adapters]

- [File and socket BIO adapters](../../../core/libs/prx/libSceLibreSsl/src/BioFileSocket.cpp) (`BIO_s_file`, `BIO_new_fp`, `BIO_new_socket`) extend #9536's guest-ABI memory BIOs with guest libc `FileStream` and `libSceNet` descriptors; ownership follows the caller's close flag. Firmware and console behavior is unverified. Socket retry flags follow LibreSSL 2.5.4's pinned [bss_sock.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/bio/bss_sock.c#L131-L238): clear retry bits after I/O, then set retry-read/write for guest EINTR, EAGAIN, EINPROGRESS, EALREADY or ENOTCONN. The sceNet errno slot is cleared before I/O so zero-byte successes cannot reuse a stale error. Socket pending-byte queries and extended controls remain unimplemented; unsupported method controls return 0. No new transport or handshake support is claimed.
