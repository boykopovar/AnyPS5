# libSceNet

## add Functional

- [sceNetInetPton source access](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - an inaccessible non-null source returns `EFAULT` without writing the destination as a host-safety fallback; the console's response is unmeasured. Checked reads stop at the first NUL or ASCII whitespace. IPv4 and dotted-tail IPv6 ignore the whitespace suffix, while hexadecimal IPv6 rejects it, preserving the existing text rules. Null source, unsupported family and null IPv6 destination checks precede source access. `sceNetInetPtonWithScopeId` also copies its complete source into host memory before splitting the optional scope name and calling this parser. Closes when: source-fault behaviour is measured on PS5.
