# libSceAgcDriver

## add Functional

- [Initial multisample color imports under a compressed CMASK](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) (libSceAgcDriver) - a target whose CMASK holds only FMASK compression (COMPRESSION without FAST_CLEAR, no DCC) imports its initial contents only when all its fragments hold one texel, as that texel; differing fragments throw. Seen in two title runs over all-zero color memory; what the console reads where an FMASK names no stored fragment is unverified on console. Low-level view construction, proven DCC/CMASK clears and sampled-image layouts keep their existing behavior. Console equivalence remains unmeasured.
