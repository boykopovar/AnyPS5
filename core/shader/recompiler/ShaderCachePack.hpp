#pragma once
#include "ShaderDiskCache.hpp"
#include <array>

namespace ShaderRecompiler::ShaderDiskCache {

inline constexpr std::uint64_t PackMagic = 0x314b434150535041ull;
struct PackHeader {
    std::uint64_t magic = PackMagic;
    std::uint64_t sourceVersion = 0;
    std::uint64_t fileBytes = 0;
    std::uint64_t entryCount = 0;
    std::uint64_t indexHash = 0;
};
struct PackRecord {
    std::array<std::uint64_t, 2> hash{};
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};
static_assert(sizeof(PackHeader) == 40 && sizeof(PackRecord) == 32);

struct PackLoadTimings {
    std::uint64_t lookupNanoseconds = 0;
    std::uint64_t decodeNanoseconds = 0;
    std::uint64_t bytes = 0;
};

[[nodiscard]] std::array<std::uint64_t, 2> PackKeyHash(std::span<const std::byte> key);
[[nodiscard]] std::vector<std::filesystem::path> PrebuiltCachePaths(const std::filesystem::path& cacheRoot);

class CachePack {
public:
    explicit CachePack(const std::filesystem::path& path);
    ~CachePack();
    CachePack(const CachePack&) = delete;
    CachePack& operator=(const CachePack&) = delete;
    [[nodiscard]] bool Valid() const { return error.empty(); }
    [[nodiscard]] const std::string& Error() const { return error; }
    [[nodiscard]] std::size_t Count() const { return records.size(); }
    [[nodiscard]] LoadStatus Load(std::span<const std::byte> key, CompiledVariant& variant, PackLoadTimings& timings) const;

private:
    void Validate();
    std::span<const std::byte> file;
    std::span<const PackRecord> records;
    std::string error;
    std::shared_ptr<const void> owner;
};

}
