#include "BdaAbi.hpp"
#include "NarrowConstantStoreFixture.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <span>
#include <string>
#include <vector>
#include <spirv/unified1/spirv.hpp>
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include <spirv-tools/libspirv.hpp>
#endif

namespace {

using namespace ShaderRecompiler;

void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

SpirvTarget DumpTarget() {
    static constexpr std::array<std::uint32_t, 4> capabilities{
        spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance,
    };
    static constexpr std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer",
    };
    return {0x00401000u, 0x00010300u, 64, BdaAbi::Version, capabilities, extensions, false,
            {1024, 1024, 64}, 1024, 32768, {}, {}};
}

std::vector<std::uint32_t> ReadWords(const std::string& path) {
    auto* file = std::fopen(path.c_str(), "rb");
    Check(file != nullptr, "the dump file " + path + " was not written");
    std::vector<std::uint32_t> words;
    std::uint32_t word = 0;
    while (std::fread(&word, sizeof(word), 1, file) == 1) words.push_back(word);
    std::fclose(file);
    return words;
}

}

int main() {
    std::string path;
    try {
#ifdef _WIN32
        _putenv_s("APS5_DUMP_SPIRV", "all");
#else
        if (setenv("APS5_DUMP_SPIRV", "all", 1) != 0) throw std::runtime_error("cannot set APS5_DUMP_SPIRV");
#endif
        const std::span<const std::uint32_t> code(NarrowConstantStoreFixture::Code);
        const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const std::array<std::uint32_t, 2> userData{0x10000u, 0u};
        const ShaderComputeStageInfo compute{{NarrowConstantStoreFixture::Threads, 1, 1}, 0, {false, false, false}, false, 1, {}};
        RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
            DumpTarget(), {0, 0, 0, 128}, std::nullopt, false,
        };
        request.useCache = false;
        const auto result = Recompile(request);
        char address[32];
        std::snprintf(address, sizeof(address), "%llx", static_cast<unsigned long long>(request.shader.codeAddress));
        char file[128];
        std::snprintf(file, sizeof(file), "spirv-%d-%s.spv", static_cast<int>(request.shader.stage), address);
        path = file;
        const auto words = ReadWords(path);
        std::remove(path.c_str());
        Check(!words.empty() && words[0] == spv::MagicNumber, "the dump file has no SPIR-V magic number");
        for (std::size_t offset = 5; offset < words.size();) {
            const auto count = words[offset] >> spv::WordCountShift;
            Check(count != 0 && count <= words.size() - offset, "malformed SPIR-V instruction in the dump file");
            offset += count;
        }
        Check(!result.spirv.Words().empty(), "the compiled module is empty");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_1);
        Check(validator.Validate(words.data(), words.size()), "the dump file is not valid SPIR-V");
#endif
        std::puts("SPIR-V dump test passed");
        return 0;
    } catch (const std::exception& error) {
        if (!path.empty()) std::remove(path.c_str());
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
