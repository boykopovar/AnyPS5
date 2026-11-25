# libSceResourceArbitrator

## add Functional

- [Pre-init, shared-memory, and feature list checks](../../../core/libs/prx/libSceResourceArbitrator/Export.cpp) - 14 exports updated from firmware 13.60 `libSceResourceArbitrator.sprx` disassembly (not run on a console): 8 IPMI-dependent exports return `0x819b0002` when uninitialized and throw via `NotImplemented_nid_no_patch` after init; 4 shared-memory exports return `0x819b0001` while `/SceRASharedMemory` is unmapped (`rMJnXXj6GIo` also rejects `id == -1` with `0x819b0003`); `TYiIrKTzQ8s` returns `0`; `cT1vA0DTMcs` locally matches up to 5 32-byte feature IDs starting at `+0x18`.
