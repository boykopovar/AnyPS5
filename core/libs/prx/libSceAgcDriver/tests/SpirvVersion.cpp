#include "SpirvBackend/SpirvOptimizer.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <span>
#include <spirv-tools/libspirv.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void Check(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void SwapWords(std::vector<std::uint32_t>& words) {
    for (auto& word : words) {
        auto bytes = std::as_writable_bytes(std::span(&word, 1));
        std::reverse(bytes.begin(), bytes.end());
    }
}

std::vector<std::uint32_t> Fixture(std::uint32_t version) {
    spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_0);
    std::vector<std::uint32_t> words;
    Check(tools.Assemble(R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
%void = OpTypeVoid
%function = OpTypeFunction %void
%main = OpFunction %void None %function
%entry = OpLabel
OpReturn
OpFunctionEnd
)", &words), "failed to assemble compute fixture");
    words[1] = version;
    return words;
}

void Reject(std::span<const std::uint32_t> words, std::uint32_t vulkanVersion, std::uint32_t spirvVersion, std::string_view message) {
    try {
        static_cast<void>(ValidateAndOptimizeSpirv(words, vulkanVersion, spirvVersion));
    } catch (const std::runtime_error& error) {
        Check(std::string_view(error.what()).starts_with(message), error.what());
        return;
    }
    throw std::runtime_error("invalid module was accepted");
}

void CheckTarget(std::uint32_t vulkanVersion, std::uint32_t spirvVersion) {
    spv_target_env environment;
    Check(spvParseVulkanEnv(vulkanVersion, spirvVersion, &environment), "invalid test target");
    spvtools::SpirvTools tools(environment);
    const char* mode = std::getenv("APS5_SPIRV_OPT");
    const bool none = mode != nullptr && std::string_view(mode) == "none";
    for (std::uint32_t version = 0x00010000u; version <= spirvVersion; version += 0x100u) {
        const auto fixture = Fixture(version);
        auto native = fixture;
        auto swapped = fixture;
        SwapWords(swapped);
        const auto swappedFixture = swapped;
        Check(tools.Validate(native) && tools.Validate(swapped), "invalid version fixture");
        const auto nativeOutput = ValidateAndOptimizeSpirv(native, vulkanVersion, spirvVersion);
        const auto swappedOutput = ValidateAndOptimizeSpirv(swapped, vulkanVersion, spirvVersion);
        Check(native == fixture && swapped == swappedFixture, "optimizer modified the input");
        if (none) {
            Check(nativeOutput == fixture && swappedOutput == swappedFixture, "none mode changed the module");
        } else {
            Check(nativeOutput == swappedOutput, "byte order changed optimized output");
            Check(tools.Validate(nativeOutput), "invalid optimizer output");
        }
    }
    auto newer = Fixture(spirvVersion + 0x100u);
    for (const bool swapped : {false, true}) {
        if (swapped) SwapWords(newer);
        Reject(newer, vulkanVersion, spirvVersion, "SPIRV-Tools: module version exceeds requested SPIR-V target");
    }
}

void CheckMalformed() {
    auto words = Fixture(0x00010300u);
    for (const bool swapped : {false, true}) {
        if (swapped) SwapWords(words);
        for (std::size_t count = 0; count <= 5; ++count) {
            Reject(std::span(words).first(count), 0x00401000u, 0x00010300u, "SPIR-V validation before optimization failed:");
        }
        for (const auto version : {0u, 0x00010001u}) {
            auto invalidVersion = Fixture(version);
            if (swapped) SwapWords(invalidVersion);
            Reject(invalidVersion, 0x00401000u, 0x00010300u, "SPIR-V validation before optimization failed:");
        }
        auto invalidMagic = words;
        invalidMagic[0] = 0;
        invalidMagic[1] = 0x00010300u;
        Reject(invalidMagic, 0x00401000u, 0x00010300u, "SPIR-V validation before optimization failed:");
        auto invalidInstruction = words;
        invalidInstruction[5] = 0;
        Reject(invalidInstruction, 0x00401000u, 0x00010300u, "SPIR-V validation before optimization failed:");
    }
}

}

int main() {
    constexpr std::array<std::array<std::uint32_t, 2>, 8> targets{{
        {0x00400000u, 0x00010000u},
        {0x00401000u, 0x00010100u},
        {0x00401000u, 0x00010300u},
        {0x00401000u, 0x00010400u},
        {0x00402000u, 0x00010300u},
        {0x00402000u, 0x00010500u},
        {0x00403000u, 0x00010600u},
        {0x00404000u, 0x00010600u},
    }};
    int failures = 0;
    for (const auto& target : targets) {
        try {
            CheckTarget(target[0], target[1]);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Vulkan 0x%08x / SPIR-V 0x%08x: %s\n", target[0], target[1], error.what());
            ++failures;
        }
    }
    try {
        CheckMalformed();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "malformed module: %s\n", error.what());
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}
