#ifndef CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_DYNAMICLINKER_HPP
#define CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_DYNAMICLINKER_HPP

#include <dlfcn.h>
#include <link.h>

inline constexpr int RTLD_DL_LINKMAP = 2;

extern "C" int dladdr1(const void* address, Dl_info* info, void** extra, int flags);

#endif
