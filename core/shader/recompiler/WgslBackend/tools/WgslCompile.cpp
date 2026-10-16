#include "ControlFlow/RequestSerializer.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Recompiler.hpp"
#include "WgslBackend/WgslTarget.hpp"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot read " + path.string());
    std::ostringstream stream;
    stream << file.rdbuf();
    auto text = stream.str();
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
    return text;
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary);
    if (!file || !(file << text)) throw std::runtime_error("cannot write " + path.string());
}

struct Options {
    bool ir = false;
    std::optional<std::filesystem::path> output;
    std::vector<std::filesystem::path> draws;
};

Options ParseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--ir") {
            options.ir = true;
        } else if (argument == "-o") {
            if (++index >= argc) throw std::runtime_error("-o needs a directory");
            options.output = argv[index];
        } else {
            options.draws.emplace_back(argument);
        }
    }
    if (options.draws.empty()) throw std::runtime_error("usage: anyps5-wgsl [--ir] [-o <directory>] <capture prefix>...");
    return options;
}

ShaderRecompiler::ShaderPixelStageInfo DefaultUnexportedInputs(ShaderRecompiler::ShaderPixelStageInfo pixel, std::span<const std::uint32_t> parameterExports) {
    for (std::uint32_t input = 0; input < pixel.interpolatorCount && input < pixel.interpolatorSettings.size(); ++input) {
        auto& setting = pixel.interpolatorSettings[input];
        if ((setting & 0x20u) != 0u || std::ranges::find(parameterExports, setting & 0x1fu) != parameterExports.end()) continue;
        setting = (setting & ~0x700u) | 0x20u;
        if ((setting & 0x80000u) != 0u) setting = (setting & ~0x600000u) | 0x100000u;
    }
    return pixel;
}

ShaderRecompiler::RecompileResult Compile(ShaderRecompiler::RecompileRequest& request, bool ir, std::map<std::string_view, std::size_t>& opcodes) {
    request.target = ShaderRecompiler::WebGpuTarget();
    request.layout = {0, 0, 0, 0};
    request.useCache = false;
    if (ir) {
        const auto program = ShaderRecompiler::PrepareResourceProgram(request);
        for (const auto& block : program.Blocks()) {
            for (const auto* value : block->Instructions()) ++opcodes[ShaderRecompiler::IrOpcodeName(value->Opcode())];
        }
    }
    return ShaderRecompiler::Recompile(request);
}

void Report(const std::filesystem::path& path, const ShaderRecompiler::RecompileResult& result) {
    std::printf("%s: %zu bytes, %zu bindings", path.string().c_str(), result.wgsl.size(), result.bindings.size());
    for (const auto& binding : result.bindings) std::printf(" [%u x%u role %u]", binding.binding, binding.count, static_cast<unsigned>(binding.role));
    std::printf(", %zu specialization constants\n", result.specialization.size());
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = ParseOptions(argc, argv);
        if (options.output) std::filesystem::create_directories(*options.output);
        std::map<std::string_view, std::size_t> opcodes;
        const ShaderRecompiler::RequestSerializer serializer;
        std::size_t failures = 0;
        for (const auto& prefix : options.draws) {
            const auto name = prefix.filename().string();
            const auto stagePath = [&](const char* suffix) { return std::filesystem::path(prefix.string() + suffix); };
            try {
                auto vertex = serializer.Deserialize(ReadFile(stagePath(".vs.b64")));
                const auto vertexResult = Compile(vertex.request, options.ir, opcodes);
                Report(stagePath(".vs.b64"), vertexResult);
                if (options.output) WriteFile(*options.output / (name + ".vs.wgsl"), vertexResult.wgsl);
                if (!std::filesystem::exists(stagePath(".ps.b64"))) continue;
                auto pixel = serializer.Deserialize(ReadFile(stagePath(".ps.b64")));
                auto& pixelInfo = pixel.request.context.pixel;
                if (!pixelInfo) throw std::runtime_error("the pixel request has no pixel stage info");
                *pixelInfo = DefaultUnexportedInputs(*pixelInfo, vertexResult.parameterExports);
                const auto pixelResult = Compile(pixel.request, options.ir, opcodes);
                Report(stagePath(".ps.b64"), pixelResult);
                if (options.output) WriteFile(*options.output / (name + ".ps.wgsl"), pixelResult.wgsl);
            } catch (const std::exception& error) {
                ++failures;
                std::fprintf(stderr, "%s: %s\n", name.c_str(), error.what());
            }
        }
        for (const auto& [opcode, count] : opcodes) std::printf("%6zu %.*s\n", count, static_cast<int>(opcode.size()), opcode.data());
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "anyps5-wgsl: %s\n", error.what());
        return 1;
    }
}
