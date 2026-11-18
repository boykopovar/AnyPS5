# libSceIcu

## add Functional

- [Resource iterator availability](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) has no console ABI or behavior verification. `ures_hasNext` takes the signature of ICU 68.2 and forwards to the pinned ICU implementation, whose null and iterator-boundary behavior is unchanged. An unreadable resource handle throws instead of faulting the host. Closes when: compared with the console.
