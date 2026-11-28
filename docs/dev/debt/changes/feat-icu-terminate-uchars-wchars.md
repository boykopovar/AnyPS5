# libSceIcu

## change [ICU buffer termination]

- [ICU buffer termination](../../../core/libs/prx/libSceIcu/TerminateBuffer.cpp) has no console ABI or cross-firmware verification. `u_terminateChars`, `u_terminateUChars`, `u_terminateUChar32s` and `u_terminateWChars` follow ICU 68.2's string termination contract: a successful call writes a terminator when there is room, exact capacity reports `U_STRING_NOT_TERMINATED_WARNING`, and excess length reports `U_BUFFER_OVERFLOW_ERROR`. `u_terminateWChars` writes a 2-byte terminator, taking the guest `wchar_t` as 16 bits as the `libSceLibcInternal` wide strings do. Their names hash to the existing 3.20 NIDs, but that does not verify console prototypes. A null destination throws only when a terminator would be written. Closes when: compared with the console.
