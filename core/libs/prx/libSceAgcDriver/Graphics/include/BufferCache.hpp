#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BUFFERCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BUFFERCACHE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <tuple>

namespace AgcDriver::Graphics {

class Buffer;

class BufferCache {
public:
    enum class Use : std::uint8_t { Storage, Vertex, Index16, Index32 };
    static constexpr std::size_t StorageBudget = std::size_t{256} << 20u;
    static constexpr std::size_t StorageEntries = 1024;
    static constexpr std::size_t GeometryBudget = std::size_t{1024} << 20u;
    static constexpr std::size_t GeometryEntries = 16384;
    struct Slice {
        std::shared_ptr<Buffer> buffer;
        std::size_t offset = 0;
        std::size_t bytes = 0;
        std::uint32_t derived = 0;
        explicit operator bool() const { return buffer != nullptr; }
    };
    struct Statistics {
        std::uint64_t lookups = 0;
        std::uint64_t hits = 0;
        std::uint64_t invalidations = 0;
        std::uint64_t mappingInvalidations = 0;
        std::uint64_t evictions = 0;
        std::uint64_t uploadedBytes = 0;
        std::uint64_t uploads = 0;
    };
    BufferCache() = default;
    BufferCache(const BufferCache&) = delete;
    BufferCache& operator=(const BufferCache&) = delete;
    Slice Find(std::uint64_t address, std::size_t bytes, Use use);
    void Keep(std::uint64_t address, std::uint64_t generation, std::uint64_t mappingGeneration, Slice slice, Use use);
    void NoteUpload(std::size_t bytes) { ++stats.uploads; stats.uploadedBytes += bytes; }
    const Statistics& Counters() const { return stats; }

private:
    using Key = std::tuple<std::uint64_t, Use, std::size_t>;
    struct Entry {
        std::uint64_t generation;
        std::uint64_t mappingGeneration;
        std::list<Key>::iterator recent;
        Slice slice;
    };
    struct Pool {
        std::list<Key> recency;
        std::size_t bytes = 0;
    };
    std::map<Key, Entry> entries;
    std::array<Pool, 2> pools;
    Statistics stats;
    void erase(std::map<Key, Entry>::iterator entry);
};

}

#endif
