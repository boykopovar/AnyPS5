#include "ShaderCompatibilityFixtures.hpp"
#include "WgslBackend/WgslTarget.hpp"
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

using namespace ShaderRecompiler;
using namespace ShaderRecompiler::Tests;

void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("wgsl target: " + message);
}

struct Reference {
    std::size_t keyWords;
    std::uint64_t keyHash;
    std::size_t spirvWords;
    std::uint64_t spirvHash;
    std::size_t artifactBytes;
    std::uint64_t artifactHash;
};

constexpr std::array<Reference, 3> MainReference{{
    {164, 0x9a52c45ff2c1835eull, 634, 0xcdd566151d182f98ull, 2981, 0xc70a80b2813fd364ull},
    {164, 0x9f782e639d169708ull, 749, 0xc3d5459a0c64dcddull, 3441, 0x01eed4c1203a449aull},
    {164, 0xe73e14b0d07cc526ull, 941, 0x56560620005bd9f1ull, 4209, 0x21d9303ddbb475afull},
}};

void CheckKeys(const CompatibilityProgram& program, const std::string& what) {
    const auto spirv = CompatibilityKey(program.Request(VulkanTarget(), true));
    auto wgslTarget = VulkanTarget();
    wgslTarget.wgsl = true;
    const auto wgsl = CompatibilityKey(program.Request(wgslTarget, true));
    Check(wgsl.size() == spirv.size() + 1 && wgsl != spirv, what + ": WGSL has no separate key");
}

void CheckSeparation(const CompatibilityProgram& program, std::size_t index, const std::string& what) {
    const auto fresh = Recompile(program.Request(VulkanTarget(), false));
    Check(fresh.spirv.Words().size() >= 5 && fresh.spirv.Words()[0] == spv::MagicNumber, what + ": no SPIR-V module");
    Check(fresh.wgsl.empty(), what + ": a SPIR-V target produced WGSL");
    const auto nativeKey = CompatibilityKey(program.Request(VulkanTarget(), false));
    const auto artifact = CompatibilityArtifact(fresh);
    const auto& reference = MainReference[index];
    Check(nativeKey.size() == reference.keyWords && CompatibilityHash(std::as_bytes(std::span(nativeKey))) == reference.keyHash, what + ": native key differs from main");
    Check(fresh.spirv.size() == reference.spirvWords && CompatibilityHash(std::as_bytes(fresh.spirv.Words())) == reference.spirvHash, what + ": SPIR-V differs from main");
    Check(artifact.size() == reference.artifactBytes && CompatibilityHash(artifact) == reference.artifactHash, what + ": encoded artifact differs from main");
    const auto before = Recompile(program.Request(VulkanTarget(), true));
    auto webGpu = program.Request(WebGpuTarget(), true);
    webGpu.layout = {0, 0, 0, 0};
    const auto wgsl = Recompile(webGpu);
    Check(!wgsl.wgsl.empty(), what + ": the WebGPU target produced no WGSL");
    const auto after = Recompile(program.Request(VulkanTarget(), true));
    Check(after.cacheHit, what + ": the SPIR-V request after the WGSL one was not served from the cache");
    Check(before.wgsl.empty() && after.wgsl.empty(), what + ": a cached SPIR-V result carries WGSL");
    const auto expected = CompatibilityArtifact(fresh);
    Check(CompatibilityArtifact(before) == expected, what + ": the cached SPIR-V result differs from an uncached compile");
    Check(CompatibilityArtifact(after) == expected, what + ": the SPIR-V result changed after the WebGPU target compiled the same program");
}

}

int main(int argc, char** argv) {
    try {
        if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--emit-wgsl")) throw std::runtime_error("expected --emit-wgsl <directory>");
        if (argc == 3) std::filesystem::create_directories(argv[2]);
        const auto programs = CompatibilityPrograms();
        for (std::size_t index = 0; index < programs.size(); ++index) {
            const auto what = "program " + std::to_string(index);
            CheckKeys(programs[index], what);
            CheckSeparation(programs[index], index, what);
            if (argc == 3) {
                auto request = programs[index].Request(WebGpuTarget(), false);
                request.layout = {0, 0, 0, 0};
                const auto result = Recompile(request);
                std::ofstream output(std::filesystem::path(argv[2]) / ("program-" + std::to_string(index) + ".cs.wgsl"));
                if (!(output << result.wgsl)) throw std::runtime_error("cannot write WGSL fixture");
            }
        }
        std::printf("wgsl target tests passed (%zu programs)\n", programs.size());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
