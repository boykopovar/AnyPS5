#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PASSHAZARDS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PASSHAZARDS_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

using GuestRangeList = std::span<const std::pair<std::uint64_t, std::uint64_t>>;

struct PassAccess {
    GuestRangeList reads;
    GuestRangeList writes;
    std::span<const std::pair<VkImage, bool>> images;
    std::span<const std::pair<VkImage, bool>> attachments;
    bool anyReads = false;
    bool anyWrites = false;
};

enum class PassHazard : std::uint8_t { None, ReadAfterWrite, WriteAfterWrite, WriteAfterRead, Image, UnknownRead, UnknownWrite };

class GuestRangeSet {
public:
    void Insert(std::uint64_t begin, std::uint64_t end);
    bool Overlaps(std::uint64_t begin, std::uint64_t end) const;
    bool Empty() const { return ranges.empty(); }
    std::size_t Size() const { return ranges.size(); }
    void Clear() { ranges.clear(); }

private:
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
};

class PassHazards {
public:
    PassHazard Check(const PassAccess& access, bool samePass) const;
    void Add(const PassAccess& access);
    void BeginPass() { ++pass; }
    void Clear();
    bool Empty() const;

private:
    struct Attachment {
        VkImage image;
        bool written;
        std::uint64_t pass;
    };
    bool overlapsAny(const GuestRangeSet& set, GuestRangeList ranges) const;
    GuestRangeSet reads;
    GuestRangeSet writes;
    std::vector<std::pair<VkImage, bool>> images;
    std::vector<Attachment> attachments;
    std::uint64_t pass = 0;
    bool anyReads = false;
    bool anyWrites = false;
};

}

#endif
