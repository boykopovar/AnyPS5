# libSceFreeTypeOt

## add Functional

- [Misaligned module service records](../../../core/libs/prx/libSceFreeTypeOt/src/ModuleService.cpp) (libSceFreeTypeOt) - `ft_module_get_service` refuses a module, module class, library or library module entry whose address is not a multiple of its type's alignment, as it refuses an unreadable one. The console module's behavior for such pointers has not been measured.
