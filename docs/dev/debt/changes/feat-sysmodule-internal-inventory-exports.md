# libSceSysmodule

## change [Additional callable exports](../../../core/libs/prx/libSceSysmodule/InventoryExports.cpp)

- [Additional callable exports](../../../core/libs/prx/libSceSysmodule/InventoryExports.cpp) - 5 firmware 13.60 `libSceSysmodule.sprx` entries (`0x370`, `0x410`, `0x490`, `0xf90`, `0x1010`), read from disassembly and not measured on a console; symbol names remain unknown. `5gs662NWKhE`, `9Q9uHJVfB4w` and `uQqnJudN5fY` gate on the 28-entry allowlist (`0x3e0` at `0xef00`) before forwarding to internal load/unload/is-loaded; `kMn8gDsKp9Y` and `6XFxHG0KhBg` check `id`, `flags == 0` and table membership before forwarding (`0x805A1000` for missing IDs; PS4/dev flag gates in `0x160` are unmodeled).

## add Silent stubs

- [InventoryExports](../../../core/libs/prx/libSceSysmodule/InventoryExports.cpp) (libSceSysmodule) - `kMn8gDsKp9Y` and `6XFxHG0KhBg` forward `flags == 0` to `sceSysmoduleLoadModuleInternalWithArg` and `sceSysmoduleUnloadModuleInternalWithArg`, ignoring module start/stop arguments and PS4/dev flag gates.
