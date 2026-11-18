# libSceAgcDriver

## add Silent stubs

- [ATOMIC_MEM](../../../core/libs/prx/libSceAgcDriver/Execution/src/AtomicMemory.cpp) (libSceAgcDriver) - floating-point TC operations, loop-until-compare command 1, and reserved opcodes/control fields throw. Integer 32/64-bit swap, compare-and-swap, add/subtract, signed/unsigned min/max, bit operations and bounded increment/decrement execute atomically on checked guest backing. Return/nonreturn TC encodings have the same visible memory effect for supported nonlooping commands; cache and engine hints serialize through the host driver rather than independent CP execution. Packet layout and operation encodings follow AMD PAL c5e800072a32f68b6ccc4422936d96167c6e0728. Typed release interrupts and nondefault context-shadow initialization remain separate gaps; emitting the versioned context-state synchronization sequence is not yet qualified.
