#include <Testing/Test.hpp>
#include "BdaAbi.hpp"
#include "../NarrowConstantStoreFixture.hpp"
#include "Recompiler.hpp"

#include <array>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <string>
#include <string_view>
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include <spirv-tools/libspirv.hpp>
#endif

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::Require;

void RequireStorageOnlyModule(const RecompileResult& result, std::string_view name) {
    const auto& words = result.spirv.Words();
    Require(words.size() >= 5 && words[0] == spv::MagicNumber, std::string(name) + ": missing SPIR-V module");
    bool byteType = false;
    bool store = false;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        const auto opcode = words[offset] & spv::OpCodeMask;
        Require(count != 0 && count <= words.size() - offset, std::string(name) + ": malformed SPIR-V instruction");
        if (opcode == spv::OpCapability && count == 2) {
            Require(words[offset + 1] != spv::CapabilityInt8, std::string(name) + ": introduced unsupported Int8 arithmetic");
        }
        if (opcode == spv::OpTypeInt && count == 4 && words[offset + 2] == 8) byteType = true;
        if (opcode == spv::OpStore) store = true;
        offset += count;
    }
    Require(byteType && store, std::string(name) + ": lost the byte storage regression fixture");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    std::string diagnostics;
    spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_1);
    validator.SetMessageConsumer([&](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
        if (message != nullptr) diagnostics += std::string(message) + '\n';
    });
    const bool valid = validator.Validate(words);
    Require(valid, std::string(name) + ": emitted invalid SPIR-V:\n" + diagnostics);
#endif
}

SpirvTarget StorageOnlyTarget() {
    static constexpr std::array<std::uint32_t, 3> capabilities{
        spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess,
    };
    static constexpr std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer",
    };
    return {0x00401000u, 0x00010300u, 64, BdaAbi::Version, capabilities, extensions, false,
            {1024, 1024, 64}, 1024, 32768, {}, {}};
}

void RequireComputeStores(std::uint32_t waveSize) {
    const std::span<const std::uint32_t> code(NarrowConstantStoreFixture::Code);
    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const std::array<std::uint32_t, 2> userData{0x10000u, 0u};
    const ShaderComputeStageInfo compute{{NarrowConstantStoreFixture::Threads, 1, 1}, 0, {false, false, false}, false, 1, {}};
    RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        StorageOnlyTarget(), {0, 0, 0, 128}, std::nullopt, false,
    };
    RequireStorageOnlyModule(Recompile(request), "constant stores wave" + std::to_string(waveSize));
}

const Case vertexReproducer{"Recompile_VertexByteStoreOnStorageOnlyTarget_EmitsByteStoresWithoutInt8", [] {
    alignas(256) static constexpr std::array<std::uint32_t, 14> code{
        0xbe801f00u, 0x800000ffu, 0x34u, 0xdc7519cbu, 0xb0020010u, 0x10005004u, 0xf4200100u,
        0xfa000000u, 0xbf8cc07fu, 0x7e0049c8u, 0xf80008cfu, 0u, 0xbf810000u, 0x3f800000u,
    };
    const std::span<const std::uint32_t> words(code);
    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    RecompileRequest request{};
    request.shader = {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}};
    request.target = StorageOnlyTarget();
    request.layout = {0, 0, 0, 128};
    request.context.waveSize = 64;
    request.context.memory = memory;
    request.context.userDataBaseRegister = 8;
    request.context.vertex = ShaderVertexStageInfo{};
    request.useCache = false;
    RequireStorageOnlyModule(Recompile(request), "issue #1276 vertex");
}};

const Case wave32Stores{"Recompile_Wave32NarrowConstantStores_EmitsByteStoresWithoutInt8", [] {
    RequireComputeStores(32);
}};

const Case wave64Stores{"Recompile_Wave64NarrowConstantStores_EmitsByteStoresWithoutInt8", [] {
    RequireComputeStores(64);
}};

} // namespace
