# libSceSysmodule technical debt

### Known modules

- [kModuleTable](../../../core/libs/prx/libSceSysmodule/ModuleTable.hpp) (libSceSysmodule) - module ID 0x00000132 maps to `libSceProprietaryVoiceChatHelperPrx` as verified in [#5017](https://github.com/boykopovar/AnyPS5/pull/5017) and reported in Discussion [#6344](https://github.com/boykopovar/AnyPS5/discussions/6344) for EA SPORTS FC 27 (PPSA34015).

### Functional

- [sceSysmoduleGetModuleInfoForUnwind](../../../core/libs/prx/libSceSysmodule/Export.cpp) (libSceSysmodule) - on Windows the region and file name of an address inside a loaded image are cached after the first lookup, because `VirtualQuery` on a large image section is slow. An entry is used only while the loader still reports the same image for the address (same base, time stamp and size). A page protection change inside the image after the first lookup, which would split or merge the region `VirtualQuery` reports, is not noticed. Addresses outside loaded images are queried every time
