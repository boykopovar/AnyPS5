#include <lifter/Program.hpp>

#include <lifter/Discovery.hpp>

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string_view>
#include <utility>

namespace Ps5Lift {

namespace {

constexpr std::string_view Origin = "$ORIGIN/";
constexpr std::uint64_t ImageAlignment = 0x1000000;

std::optional<std::filesystem::path> GuestModulePath(const std::string& image, const std::string& needed) {
    if (!needed.starts_with(Origin)) return std::nullopt;
    return std::filesystem::weakly_canonical(std::filesystem::path(image).parent_path() / needed.substr(Origin.size()));
}

std::map<std::string, std::uint64_t> Definitions(const GuestImageFile& image, const std::vector<GuestImageFile>& files) {
    std::map<std::string, std::uint64_t> definitions;
    for (const auto& name : image.UndefinedSymbols()) {
        if (JumpBufferFunction(name)) continue;
        for (const auto& file : files) {
            if (const auto address = file.Export(name)) {
                definitions.emplace(name, *address);
                break;
            }
        }
    }
    return definitions;
}

}

GuestProgram GuestProgram::Load(const std::string& executable) {
    std::vector<GuestImageFile> files;
    files.push_back(GuestImageFile::Read(executable, std::nullopt));
    std::map<std::filesystem::path, std::size_t> loaded;
    std::vector<std::vector<std::size_t>> dependencies(1);
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto path = files[index].Path();
        const auto needed = files[index].Needed();
        for (const auto& name : needed) {
            const auto module = GuestModulePath(path, name);
            if (!module) continue;
            const auto [found, inserted] = loaded.emplace(*module, files.size());
            if (inserted) {
                const auto base = (files.back().End() + ImageAlignment - 1) & ~(ImageAlignment - 1);
                files.push_back(GuestImageFile::Read(module->string(), base));
                dependencies.emplace_back();
            }
            dependencies[index].push_back(found->second);
        }
    }

    std::vector<std::map<std::string, std::uint64_t>> definitions;
    for (const auto& file : files) definitions.push_back(Definitions(file, files));
    GuestProgram program;
    std::uint64_t tlsOffset = 0;
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto& tls = files[index].Tls();
        GuestTlsPlacement placement;
        if (tls.MemorySize != 0) {
            tlsOffset = (tlsOffset + tls.MemorySize + tls.Alignment - 1) / tls.Alignment * tls.Alignment;
            placement = {index + 1, tlsOffset};
        }
        program.Images.emplace_back(std::move(files[index]), placement, definitions[index]);
    }

    std::vector<bool> visited(program.Images.size());
    const std::function<void(std::size_t)> visit = [&](const std::size_t index) {
        visited[index] = true;
        for (const auto dependency : dependencies[index]) {
            if (!visited[dependency]) visit(dependency);
        }
        if (index != 0) program.Initialization.push_back(index);
    };
    visit(0);
    return program;
}

std::string GuestProgram::LoaderName(const std::size_t index) const {
    if (index == 0) return "";
    return std::filesystem::relative(Images[index].Path(), std::filesystem::absolute(Images.front().Path()).parent_path()).generic_string();
}

}
