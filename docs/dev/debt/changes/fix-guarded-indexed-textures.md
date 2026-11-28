# shader

## change [flat SRT read]

- A [flat SRT read](../../../core/shader/recompiler/Optimization/src/SrtWalker/SrtDescriptorEvaluation.cpp) whose base or scalar offset is loaded from memory raises a [BDA fault](../../../core/libs/prx/libSceAgcDriver/Graphics/src/BdaResources.cpp) only if the program executes it, then stops or reads zero like a BDA read. [Address arithmetic](../../../core/shader/recompiler/Optimization/src/SrtWalker/SrtEvaluator.cpp) outside 48 bits reports its address modulo 2^48 (`0xfffffffffffc` for offset -4 from null, `0x20` for offset 64 from `0xffffffffffe0`); the address the console's fault reports for it is not measured. Runtime-indexed T# coverage uses synthetic compute readback and fragment capture and compilation; PPSA99903 stage loading remains unverified.

## change A value read through a null or unmapped pointer still throws

- A value read through a null or unmapped pointer still throws at record time if a [uniform fill or table base](../../../core/shader/recompiler/Optimization/src/SrtWalker/SrtDescriptorEvaluation.cpp) needs it, or if its non-null base comes from user data and neither its base nor scalar offset is loaded from memory, even if the console skips the read.
