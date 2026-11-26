# libSceLibreSsl

## add Functional

- [SSL_CTX_set1_groups_list](../../../core/libs/prx/libSceLibreSsl/src/SslGroups.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 2.5.4 `t1_lib.c`: a colon-separated list is resolved through the group short names, the two long names that differ from them and the five NIST names, mapped to the TLS group identifiers and stored on the context; an unknown, empty or unterminated list fails and keeps the previous list, and the old list is freed when replaced and with the context. The accepted names are the 29 groups LibreSSL maps, and a list longer than 1 MiB throws. Closes when: compared with the console.
