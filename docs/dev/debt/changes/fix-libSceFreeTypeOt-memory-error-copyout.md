# libSceFreeTypeOt

## add Functional

- [ft_mem_qalloc, ft_mem_alloc, ft_mem_qrealloc, ft_mem_realloc, ft_mem_dup and ft_mem_strdup](../../../core/libs/prx/libSceFreeTypeOt/src/Memory.cpp) (libSceFreeTypeOt) - error output pointers are validated upfront under `GuestArena::HostWrite` with `GuestWritable_nid_no_patch` and written via `GuestCopyOut_nid_no_patch` with caller `errno` preserved; throwing `NotImplemented_nid_no_patch` on null or unwritable error pointers is a host-safety fallback with console behavior unmeasured. Closes when: measured on hardware. Labels: console, tested.
