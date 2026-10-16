#include "Shaders.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

namespace WasmGpu {
namespace {

void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("AGC driver: " + reason);
}

std::mutex g_mutex;
std::map<std::uint64_t, std::vector<std::shared_ptr<const ShaderBinary>>> g_shaders;

std::optional<ShaderRecompiler::ShaderFloatMode> FloatMode(const Shader& fields) {
    std::uint32_t rsrc1;
    std::uint32_t fp16OverflowBit;
    switch (fields.type) {
    case 0: rsrc1 = 0x212; fp16OverflowBit = 26; break;
    case 1: rsrc1 = 0x00a; fp16OverflowBit = 29; break;
    case 2: case 4: case 6: rsrc1 = 0x08a; fp16OverflowBit = 31; break;
    case 5: case 7: rsrc1 = 0x10a; fp16OverflowBit = 30; break;
    default: return std::nullopt;
    }
    if (fields.num_sh_registers == 0) return std::nullopt;
    AgcDriver::GuestMemory::CheckRange(fields.sh_registers, fields.num_sh_registers * sizeof(ShaderRegister), alignof(ShaderRegister));
    for (std::size_t index = 0; index < fields.num_sh_registers; ++index) {
        const auto entry = fields.sh_registers[index];
        if (entry.offset != rsrc1) continue;
        const auto value = entry.value;
        return ShaderRecompiler::ShaderFloatMode{(value >> 12u) & 0xffu, ((value >> 21u) & 1u) != 0u, ((value >> 23u) & 1u) != 0u, ((value >> fp16OverflowBit) & 1u) != 0u};
    }
    return std::nullopt;
}

}  // namespace

void RegisterShader(const Shader* shader) {
    AgcDriver::GuestMemory::CheckRange(shader, sizeof(Shader), 1);
    Shader fields;
    std::memcpy(&fields, static_cast<const void*>(shader), sizeof(Shader));
    Require(fields.file_header == 0x34333231u && fields.version == 0x18u, "invalid shader header");
    Require(fields.header_size >= sizeof(Shader), "shader header is smaller than its fixed fields");
    Require(fields.shader_size != 0 && (fields.shader_size & 3u) == 0, "invalid shader size");
    AgcDriver::GuestMemory::CheckRange(shader, fields.header_size, 1);
    const auto* code = const_cast<const void*>(fields.code);
    AgcDriver::GuestMemory::CheckRange(code, fields.shader_size, 256);
    auto binary = std::make_shared<ShaderBinary>();
    binary->codeAddress = reinterpret_cast<std::uintptr_t>(code);
    binary->headerAddress = reinterpret_cast<std::uintptr_t>(shader);
    binary->type = fields.type;
    binary->code.resize(fields.shader_size / sizeof(std::uint32_t));
    std::memcpy(binary->code.data(), code, fields.shader_size);
    binary->header.resize(fields.header_size);
    std::memcpy(binary->header.data(), static_cast<const void*>(shader), fields.header_size);
    binary->floatMode = FloatMode(fields);
    std::lock_guard lock(g_mutex);
    auto& entries = g_shaders[binary->codeAddress];
    for (const auto& current : entries) {
        if (current->headerAddress == binary->headerAddress && current->type == binary->type && current->code == binary->code && current->header == binary->header) return;
    }
    std::erase_if(entries, [&](const auto& current) { return current->headerAddress == binary->headerAddress; });
    entries.push_back(std::move(binary));
}

std::shared_ptr<const ShaderBinary> FindShader(std::uint64_t address, std::initializer_list<std::uint8_t> types) {
    std::lock_guard lock(g_mutex);
    auto found = g_shaders.upper_bound(address);
    Require(found != g_shaders.begin(), "graphics program does not belong to a registered shader");
    --found;
    std::shared_ptr<const ShaderBinary> result;
    for (const auto& binary : found->second) {
        if (address - binary->codeAddress >= binary->code.size() * sizeof(std::uint32_t)) continue;
        if (std::ranges::find(types, binary->type) == types.end()) continue;
        Require(result == nullptr || result->type == binary->type, "registered program has ambiguous shader binary types");
        result = binary;
    }
    Require(result != nullptr, "graphics program does not belong to a compatible registered shader");
    return result;
}

}  // namespace WasmGpu
