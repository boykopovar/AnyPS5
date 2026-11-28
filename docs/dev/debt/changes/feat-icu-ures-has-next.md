# libSceIcu

## add Functional

- [Resource iterator availability](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) has no console ABI or behavior verification. `ures_hasNext` takes the signature of ICU 68.2 and compares a safely copied iterator index and size with the pinned ICU implementation's boundaries. An unreadable iterator state throws instead of faulting the host. Closes when: compared with the console.
