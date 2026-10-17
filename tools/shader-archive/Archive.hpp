#pragma once
#include "Common.hpp"

namespace ShaderArchive {
struct Entry {
    std::string library, container, ueHash;
    std::size_t shaderIndex = 0, group = 0, offset = 0, size = 0;
    unsigned frequency = 0;
};
struct GroupData {
    std::string id;
    std::shared_ptr<Bytes> bytes;
};
struct Archive {
    std::vector<Entry> entries;
    std::vector<GroupData> groups;
    Json::Value report;
};
Bytes Decompress(View input, std::size_t rawSize);
Archive Extract(const fs::path &paks, unsigned workers, const fs::path &output = {});
Archive LoadExtracted(const fs::path &root, unsigned workers);
} // namespace ShaderArchive
