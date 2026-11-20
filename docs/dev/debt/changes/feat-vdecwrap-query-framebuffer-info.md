# libSceVdecwrap

## add Functional

- [sceVdecwrapQueryFrameBufferInfo](../../../core/libs/prx/libSceVdecwrap/Export.cpp) (libSceVdecwrap) - behavior reconstructed from firmware export `-60wFf6p8c0` at VA `0x550`: validates the wrapper structures, converts the codec/config fields to `VdecCoreConfigInfo`, calls `sceVdecCoreQueryFrameBufferInfo`, and copies the first four 32-bit result fields to the wrapper output after its 8-byte size header. The wrapper's error translation follows the firmware branches. Inaccessible guest output is mapped to the wrapper's generic invalid-argument error as a host-safety fallback.
