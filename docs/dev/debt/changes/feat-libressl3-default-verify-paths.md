# libSceLibreSsl3

## add Functional

- [SSL_CTX_set_default_verify_paths](../../../core/libs/prx/libSceLibreSsl3/src/SslCtxDefaultVerifyPaths.cpp) (libSceLibreSsl3) - not checked on the console. LibreSSL 4.0.0 `ssl_lib.c` returns 1 lazily without reading; this export is eager. `SSL_CERT_FILE` and `SSL_CERT_DIR` override; otherwise the first found of `/etc/ssl/certs/ca-certificates.crt`, `/etc/pki/tls/certs/ca-bundle.crt`, `/etc/ssl/cert.pem`, `/opt/homebrew/etc/openssl@3/cert.pem`, `/usr/local/etc/openssl@3/cert.pem`, `/usr/local/etc/openssl/cert.pem`, `/usr/local/etc/ssl/cert.pem` in that order and the `/etc/ssl/certs` directory when it exists are loaded separately, so an unreadable bundle still registers the directory. 1 only when one loaded, 0 for a null context, null `certStore` or neither; a missing directory is not counted, and its certificates stay lazy. Candidates are guest paths under the run directory, not the host's; console paths are unverified. Closes when: compared with the console.

## remove `DZ6fywqpaik`
