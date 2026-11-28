# libSceSysmodule

## change [Additional callable exports](../../../core/libs/prx/libSceSysmodule/InventoryExports.cpp)

- [Additional callable exports](../../../core/libs/prx/libSceSysmodule/InventoryExports.cpp) - `kMn8gDsKp9Y` and `6XFxHG0KhBg` follow firmware 13.60 `libSceSysmodule.sprx` disassembly (`0xf90`, `0x1010`), not measured on a console; symbol names remain unknown. Both check `id`, `flags == 0`, and `kModuleTable` membership before forwarding (`0x805A1000` for missing IDs; PS4/dev flag gates in `0x160` are unmodeled). The remaining 3 `libSceSysmodule_jvm` entries (`5gs662NWKhE`, `9Q9uHJVfB4w`, `uQqnJudN5fY`) remain parameterless `NotImplemented_nid_no_patch` import-resolution stubs pending console measurement of accepted IDs.

## add Silent stubs

- [InventoryExports](../../../core/libs/prx/libSceSysmodule/InventoryExports.cpp) (libSceSysmodule) - `kMn8gDsKp9Y` and `6XFxHG0KhBg` forward `flags == 0` to `sceSysmoduleLoadModuleInternalWithArg` and `sceSysmoduleUnloadModuleInternalWithArg`, ignoring module start/stop arguments and PS4/dev flag gates.
