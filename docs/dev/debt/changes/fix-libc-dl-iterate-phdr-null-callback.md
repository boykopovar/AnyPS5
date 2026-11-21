# libc

## add Unknown function info

- [dl_iterate_phdr](../../../core/libs/prx/libc/src/specifics/linux/DlIteratePhdr.cpp) (libc) throws `std::invalid_argument` when `callback` is null as a host-safety fallback instead of forwarding a null pointer to host `dl_iterate_phdr` (where Linux, Emscripten, and the console fault on dereference, macOS returns 0 when no guest `__APS5DATA/__meta` image is loaded, and Windows previously threw `std::runtime_error`). Labels: code, tested, linux, macos, windows
