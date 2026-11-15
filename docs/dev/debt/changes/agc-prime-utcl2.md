# libSceAgc

## remove [sceAgcDcbPrimeUtcl2]

# libSceAgcDriver

## add Functional

- [PRIME_UTCL2](../../../core/libs/prx/libSceAgcDriver/Execution/src/Pm4.cpp) - guest page-translation cache prefetch and acknowledgement timing are not modelled. The driver validates the measured five-dword packet without touching guest memory; host GPU virtual-memory translation is managed by the host driver. Packet bytes were compared with the pinned console oracle; no new console timing measurement was made.
