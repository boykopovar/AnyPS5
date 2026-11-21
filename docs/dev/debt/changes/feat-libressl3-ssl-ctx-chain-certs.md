# libSceLibreSsl3

## add Functional

- [SSL_CTX_add0_chain_cert and SSL_CTX_clear_chain_certs](../../../core/libs/prx/libSceLibreSsl3/src/SslCtxChainCerts.cpp) (libSceLibreSsl3) - `SSL_CTX_add0_chain_cert` appends an X509 certificate to `context->extraCerts` without incrementing reference count on success. On failure (null arguments, stack allocation failure, or push failure), ownership is retained by caller per OpenSSL add0 contract; new stacks on push failure are freed and `context->extraCerts` reset to null. `SSL_CTX_clear_chain_certs` validates stack metadata, detaches `context->extraCerts` to null before releasing each certificate via `ReleaseCertificate` decrementing references and invoking `FreeCertificate_nid_no_patch` on final reference while guarding nonpositive references, and frees stack via `sk_free`, returning 1 on success or 0 if context is null or metadata is invalid. Unverified on console.
