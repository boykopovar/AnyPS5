# libSceLibreSsl3

## add Functional

- [SSL_set_accept_state](../../../core/libs/prx/libSceLibreSsl3/ParentExports.cpp) - the export forwards to libSceLibreSsl's implementation like `SSL_set_connect_state`, so it acts on the parent's LibreSSL 2.5.4 layout rather than the 4.0 layout of this module's other SSL accessors: it sets `server`, clears `shutdown`, stores the accept-before state `0x6000` and the method's accept handler, and throws when a cipher or hash context is live. The PS5 layout, the forwarding contract and console behaviour are unverified. Closes when: compared with the console.

## remove `5BoCLZK9mGk`

# libSceLibreSsl

## change [SSL_set_verify, SSL_get0_alpn_selected, SSL_set_connect_state, SSL_ctrl]

- [SSL_set_verify, SSL_get0_alpn_selected, SSL_set_connect_state, SSL_set_accept_state, SSL_ctrl](../../../core/libs/prx/libSceLibreSsl/src/Ssl.cpp) (libSceLibreSsl) - nothing is checked on the console. `SSL_ctrl` throws for the commands LibreSSL passes to `ssl3_ctrl` or `dtls1_ctrl`, including `SSL_set_tlsext_host_name`. `SSL_set_connect_state` and `SSL_set_accept_state` set `server` to 0 and 1, clear shutdown, store the `0x5000`/`0x6000` state and the method's connect/accept handler, and throw on a live cipher or hash context. Layouts and results follow [LibreSSL 2.5.4](https://github.com/libressl/openbsd/tree/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl) `ssl.h`, `ssl_locl.h` and `ssl_lib.c`, the version in the [firmware 13.40 measurements](https://github.com/boykopovar/AnyPS5/discussions/8981#discussioncomment-19111214). No export creates a connection or `SSL_METHOD` yet, so only structures the caller builds reach them. Null pointers that LibreSSL dereferences throw
