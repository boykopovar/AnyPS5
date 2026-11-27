# relinker

## change with duplicate guest exports still use flat symbol lookup on macOS

- [Module-qualified guest imports](../../../core/relinker/relinker/src/guest/GuestModuleBuilder.cpp) with duplicate guest exports bind through per-slot two-level Mach-O dylib ordinals on macOS: the executable and each guest module record the requested provider's 1-based `LC_LOAD_DYLIB` ordinal, so `BIND_SPECIAL_DYLIB_FLAT_LOOKUP` is emitted only for imports that name no provider. An unqualified macOS executable import that several needed providers export still fails conversion, as on Linux and Windows. Linux and macOS provider aliases are tested with synthetic modules, without console module-binding measurements or a title run, and the macOS images were validated structurally, not executed on Apple hardware.
