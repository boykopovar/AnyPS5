#pragma once
#include "Archive.hpp"
#include "Recompiler.hpp"
#include <map>
#include <set>

namespace ShaderArchive {
struct Native {
    std::size_t headerOffset{}, headerSize{}, codeOffset{}, codeSize{}, executableSize{};
    unsigned type{}, target{}, scratch{};
    std::map<unsigned, unsigned> sh, cx;
    std::vector<unsigned> specials, inputs, outputs, direct;
    std::string codeHash, headerHash;
};
struct Program {
    std::string id;
    std::shared_ptr<Bytes> group;
    std::size_t offset{}, size{};
    Native native;
    Json::Value aliases{Json::arrayValue};
};
std::vector<Native> ParseNative(View blob);
std::vector<Program> Programs(const Archive &archive, unsigned workers);
void WriteProgramIndex(const fs::path &path, const std::vector<Program> &programs);
std::vector<Program> LoadProgramIndex(const fs::path &index, const fs::path &extracted,
                                      const std::set<std::string> &selection, unsigned target, unsigned sample,
                                      unsigned limit, unsigned workers, Json::Value &report);
struct RequestData {
    ShaderRecompiler::RecompileRequest request{};
    std::vector<std::uint32_t> code, userData;
    Bytes header;
    std::array<ShaderRecompiler::MemoryRegion, 2> memory;
};
std::unique_ptr<RequestData> MakeRequest(const Program &program, ShaderRecompiler::SpirvTarget target,
                                         unsigned subgroup32, unsigned subgroup64);
} // namespace ShaderArchive
