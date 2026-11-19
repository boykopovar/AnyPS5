# libSceSysmodule

## add Functional

- [sceSysmoduleLoadModuleByNameInternal](../../../core/libs/prx/libSceSysmodule/Export.cpp) (libSceSysmodule) copies the module name in before it builds the candidate paths, at most the 256 bytes a path can hold, so an unreadable name returns `SCE_KERNEL_ERROR_EFAULT` (`0x8002000e`) without trying to load anything. This is a host-safety fallback: the console's result for an inaccessible name is not measured. Closes when: compared with the console.
