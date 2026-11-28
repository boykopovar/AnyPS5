# relinker

## change FS/GS bases are not modeled by the

- GS bases are not modeled by the [GOT-access index](../../../core/relinker/relinker/src/analysis/UnusedNidFilter/GotAccessIndex.cpp) or [strict reachability analysis](../../../core/relinker/relinker/src/analysis/UnusedNidFilter/StrictReachability.cpp). A reachable decoded GS memory operand retains every import, because the guest can set the GS base through `sysarch(AMD64_SET_GSBASE)` to any address, including an import slot. Decoded FS memory operands are modeled as ABI thread-pointer TLS accesses: they retain only imports whose relocation address lies inside a `PT_TLS` image, and every import when no TLS layout is modeled, so ordinary reads such as `mov rax, fs:[0]` no longer retain unrelated imports. Dead regions and prefixes on LEA, register operands and memory-form NOPs do not trigger the GS fallback. Gate-image filtering outputs have not been compared for this behavior.
