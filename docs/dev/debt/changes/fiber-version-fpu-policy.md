# libSceFiber

## change [_sceFiberInitializeWithInternalOptionImpl]

- [_sceFiberInitializeWithInternalOptionImpl](../../../core/libs/prx/libSceFiber/Export.cpp): PS5 signatures, behavior and build-version encoding remain unmeasured. Signatures and initialization follow pinned shadPS4 [implementation](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/fiber/fiber.cpp#L150-L182), [flags](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/fiber/fiber.h#L27-L32) and [version](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/common/elf_info.h#L89). Flag `0x100` selects MXCSR `0x9fc0` and x87CW `0x037f`; versions from `0x03500000` add it to caller flags. Flag `0x10` enables context-size checks. Flag `0x1` has no effect because ulobjmgr is absent. Other bits throw; `opt_param` remains ignored.
