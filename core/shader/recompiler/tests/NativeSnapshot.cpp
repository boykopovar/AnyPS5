#include "ShaderCompatibilityFixtures.hpp"
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

void Write(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("cannot write native shader snapshot: " + path.string());
}

}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected native snapshot directory");
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        const auto programs = ShaderRecompiler::Tests::CompatibilityPrograms();
        for (std::size_t index = 0; index < programs.size(); ++index) {
            const auto request = programs[index].Request(ShaderRecompiler::Tests::VulkanTarget(), false);
            const auto key = ShaderRecompiler::Tests::CompatibilityKey(request);
            const auto result = ShaderRecompiler::Recompile(request);
            const auto artifact = ShaderRecompiler::Tests::CompatibilityArtifact(result);
            const auto prefix = directory / ("program-" + std::to_string(index));
            Write(prefix.string() + ".key", std::as_bytes(std::span(key)));
            Write(prefix.string() + ".spv", std::as_bytes(result.spirv.Words()));
            Write(prefix.string() + ".artifact", artifact);
        }
        std::printf("native shader snapshots written (%zu programs)\n", programs.size());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
