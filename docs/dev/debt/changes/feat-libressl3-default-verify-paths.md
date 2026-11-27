# libSceLibreSsl3

## add Functional

- [SSL_CTX_set_default_verify_paths](../../../core/libs/prx/libSceLibreSsl3/src/SslCtxDefaultVerifyPaths.cpp) - no console check. Eager, unlike LibreSSL's lazy `X509_STORE_set_default_paths`: it loads `/system/common/cert/CA_LIST.cer` (the libkernel provider that writes the host trust store), then `SSL_CERT_FILE`/`SSL_CERT_DIR`, then the first existing guest candidate (`/etc/ssl/certs/ca-certificates.crt`, `/etc/pki/tls/certs/ca-bundle.crt`, `/etc/ssl/cert.pem`) and `/etc/ssl/certs`, returning 1 only when something loaded, 0 for a null context or store and 0 when nothing did. A failing location no longer aborts the other, and the loader now converts T61, BMP and Universal name values to UTF-8 instead of throwing. Caveats: the guest environment supplies the overrides; the context is the 120-byte libSceLibreSsl `SSL_CTX` of the forwarded `SSL_CTX_new`, `certStore` at offset 16, not the 4.0 layout; `X509_verify_cert` and `X509_STORE_free` throw. Closes when: compared with the console.

## remove `DZ6fywqpaik`
