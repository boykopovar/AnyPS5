# libSceIcu

## add Functional

- [ICU buffer termination guest access](../../../core/libs/prx/libSceIcu/TerminateBuffer.cpp) (libSceIcu) - `u_terminateChars`, `u_terminateUChars`, `u_terminateUChar32s` and `u_terminateWChars` read the status and write the terminator and status through checked copies, writing only what ICU writes, so a read-only status still works when ICU leaves it unchanged. An unreadable status or an unwritable terminator or status throws `std::invalid_argument` where ICU dereferences it, so this is a host-safety fallback with no console behaviour to match.
