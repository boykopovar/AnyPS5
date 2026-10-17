#ifndef CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_HOST_HPP
#define CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_HOST_HPP

#include <cstdint>

namespace WasmRuntime {

inline constexpr std::uint64_t TscFrequency = 1'000'000'000;

std::uint64_t ReadTsc();
const char* MainImageName();

}  // namespace WasmRuntime

#endif
