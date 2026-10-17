#ifndef CORE_WASM_RUNTIME_GPU_SHADERS_HPP
#define CORE_WASM_RUNTIME_GPU_SHADERS_HPP

#include "Recompiler.hpp"
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <vector>

struct Shader;

namespace WasmGpu {

struct ShaderBinary {
    std::uint64_t codeAddress;
    std::uint64_t headerAddress;
    std::uint8_t type;
    std::vector<std::uint32_t> code;
    std::vector<std::byte> header;
    std::optional<ShaderRecompiler::ShaderFloatMode> floatMode;
};

void RegisterShader(const Shader* shader);
std::shared_ptr<const ShaderBinary> FindShader(std::uint64_t address, std::initializer_list<std::uint8_t> types);

}  // namespace WasmGpu

#endif
