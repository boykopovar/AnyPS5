#pragma once
#include "Common.hpp"
#include "ShaderCachePack.hpp"

namespace ShaderArchive {
class PackWriter {
public:
    PackWriter(const fs::path& path, std::size_t maximumEntries);
    ~PackWriter();
    void Add(std::span<const std::byte> entry);
    Json::Value Finish();
private:
    fs::path path, temporary;
    std::ofstream output;
    std::mutex mutex;
    std::vector<ShaderRecompiler::ShaderDiskCache::PackRecord> records;
    std::size_t maximum;
    std::uint64_t offset;
    Clock::time_point started = Clock::now();
};
Json::Value PackFolder(const fs::path& source, const fs::path& destination, std::size_t limit = 0);
}
