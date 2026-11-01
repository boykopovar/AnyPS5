#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_MEMORY_WRITTENBUFFERINDEX_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_MEMORY_WRITTENBUFFERINDEX_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace AgcDriver::DriverDetail {

inline constexpr std::size_t WrittenBufferRing = 4096;

struct WrittenBuffer {
    std::uint64_t program;
    std::uint64_t begin;
    std::uint64_t end;
    std::uint64_t serial;
    std::uint32_t queue;
    bool atomic;
    std::shared_ptr<const std::vector<std::byte>> value;
    std::uint64_t generation = 0;
};

class WrittenBufferIndex {
public:
    explicit WrittenBufferIndex(std::size_t capacity = WrittenBufferRing);

    void Push(WrittenBuffer buffer);
    std::optional<WrittenBuffer> Newest(std::uint64_t begin, std::uint64_t end) const;
    const std::deque<WrittenBuffer>& Entries() const;

private:
    static constexpr unsigned GranuleBits = 16;
    static constexpr std::uint64_t WideGranules = 16;

    static std::uint64_t firstGranule(const WrittenBuffer& buffer);
    static std::uint64_t lastGranule(const WrittenBuffer& buffer);
    static bool wide(const WrittenBuffer& buffer);
    std::optional<WrittenBuffer> scan(std::uint64_t begin, std::uint64_t end) const;
    void evictOldest();

    std::size_t capacity;
    std::uint64_t evicted = 0;
    std::deque<WrittenBuffer> entries;
    std::deque<std::uint64_t> wideEntries;
    std::unordered_map<std::uint64_t, std::deque<std::uint64_t>> granules;
};

}

#endif
