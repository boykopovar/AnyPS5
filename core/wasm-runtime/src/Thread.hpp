#ifndef CORE_WASM_RUNTIME_SRC_THREAD_HPP
#define CORE_WASM_RUNTIME_SRC_THREAD_HPP

#include "wasm-runtime/GuestCpu.hpp"
#include <cstdint>

namespace WasmRuntime {

void Dispatch(GuestCpu& cpu);
std::uint64_t ThreadPointer();

}  // namespace WasmRuntime

#endif
