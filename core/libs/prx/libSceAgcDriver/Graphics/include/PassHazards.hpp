#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PASSHAZARDS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PASSHAZARDS_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <array>
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
    GuestRangeList writeOnly = {};
};

enum class PassHazard : std::uint8_t { None, ReadAfterWrite, WriteAfterWrite, WriteAfterRead, Image, UnknownRead, UnknownWrite };

enum class PassBreak : std::uint8_t { Continued, NewBatch, Command, Label, Samples, Forced, Segment, Key, ReadAfterWrite, WriteAfterWrite, WriteAfterRead, Image, UnknownRead, UnknownWrite, Count };
constexpr std::size_t PassBreakCount = static_cast<std::size_t>(PassBreak::Count);
constexpr std::array<const char*, PassBreakCount> PassBreakNames{"continued", "new batch", "command", "label", "samples", "forced", "segment", "key", "read after write", "write after write", "write after read", "image", "unknown read", "unknown write"};

constexpr PassBreak PassBreakFor(PassHazard hazard) {
    switch (hazard) {
    case PassHazard::ReadAfterWrite: return PassBreak::ReadAfterWrite;
    case PassHazard::WriteAfterWrite: return PassBreak::WriteAfterWrite;
    case PassHazard::WriteAfterRead: return PassBreak::WriteAfterRead;
    case PassHazard::Image: return PassBreak::Image;
    case PassHazard::UnknownRead: return PassBreak::UnknownRead;
    case PassHazard::UnknownWrite: return PassBreak::UnknownWrite;
    case PassHazard::None: break;
    }
    return PassBreak::Key;
}

class GuestRangeSet {
public:
    void Insert(std::uint64_t begin, std::uint64_t end);
    bool Overlaps(std::uint64_t begin, std::uint64_t end) const;
    bool Empty() const { return ranges.empty(); }
    std::size_t Size() const { return ranges.size(); }
    GuestRangeList Ranges() const { return ranges; }
    void Clear() { ranges.clear(); }

private:
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
};

class PassHazards {
public:
    PassHazard Check(const PassAccess& access, bool samePass) const;
    void Add(const PassAccess& access);
    void BeginPass() { ++pass; }
    void SealWriteOnly();
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
    GuestRangeSet writeOnly;
    std::vector<std::pair<VkImage, bool>> images;
    std::vector<Attachment> attachments;
    std::uint64_t pass = 0;
    bool anyReads = false;
    bool anyWrites = false;
};

}

#endif
