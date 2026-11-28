# libSceLibreSsl

## add Functional

- [SSL_set_session and SSL_SESSION_free](../../../core/libs/prx/libSceLibreSsl/src/SslSession.cpp) (libSceLibreSsl) - nothing is checked on the console. The session model now spells out the 232-byte LibreSSL 2.5.4 layout, with `references` at 160 and `cipher` at 168; `SSL_set_session` retains the adopted session, releases the previous one, copies its verification result and clears on a null session, and `SSL_SESSION_free` releases the last reference and frees the server-name and ticket bytes. A session with a peer certificate, a shared cipher stack or internals throws, and `SSL_set_session` does not re-select the method from the session version. Closes when: compared with the console.
