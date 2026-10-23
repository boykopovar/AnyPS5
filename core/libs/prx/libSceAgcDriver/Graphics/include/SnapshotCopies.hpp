#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SNAPSHOTCOPIES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SNAPSHOTCOPIES_HPP

#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <tuple>
#include <vector>

namespace AgcDriver::Graphics {
class Buffer;

namespace Detail {
struct SnapshotCopies {
    using Key = std::tuple<std::uint64_t, std::size_t, VkBufferUsageFlags>;
    static constexpr std::size_t Budget = std::size_t{16} << 20u;
    static constexpr std::size_t MaxEntries = 1024;
    AgcDriver::Mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    struct Entry {
        std::shared_ptr<Buffer> buffer;
        std::vector<std::byte> bytes;
        std::list<Key>::iterator recent;
    };
    std::map<Key, Entry> entries;
    std::list<Key> recency;
    std::size_t bytes = 0;

    void Insert(const Key& key, const std::shared_ptr<Buffer>& buffer, std::span<const std::byte> data) {
        recency.push_back(key);
        try {
            entries.emplace(key, Entry{buffer, {data.begin(), data.end()}, std::prev(recency.end())});
        } catch (...) {
            recency.pop_back();
            throw;
        }
        bytes += data.size();
    }

    void Clear() {
        entries.clear();
        recency.clear();
        bytes = 0;
    }
};
}
}

#endif
