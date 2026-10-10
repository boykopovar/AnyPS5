#include "prx/libSceAgcDriver/Graphics/include/PassHazards.hpp"
#include <algorithm>

namespace AgcDriver::Graphics {

void GuestRangeSet::Insert(std::uint64_t begin, std::uint64_t end) {
    if (begin >= end) return;
    auto first = std::lower_bound(ranges.begin(), ranges.end(), begin, [](const auto& range, std::uint64_t value) { return range.second < value; });
    auto last = first;
    while (last != ranges.end() && last->first <= end) {
        begin = std::min(begin, last->first);
        end = std::max(end, last->second);
        ++last;
    }
    if (first == last) {
        ranges.insert(first, {begin, end});
        return;
    }
    *first = {begin, end};
    ranges.erase(first + 1, last);
}

bool GuestRangeSet::Overlaps(std::uint64_t begin, std::uint64_t end) const {
    if (begin >= end) return false;
    const auto it = std::upper_bound(ranges.begin(), ranges.end(), begin, [](std::uint64_t value, const auto& range) { return value < range.second; });
    return it != ranges.end() && it->first < end;
}

bool PassHazards::overlapsAny(const GuestRangeSet& set, GuestRangeList ranges) const {
    if (set.Empty()) return false;
    return std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return set.Overlaps(range.first, range.second); });
}

PassHazard PassHazards::Check(const PassAccess& access, bool samePass) const {
    const auto touchesMemory = [](const PassAccess& use) {
        const auto nonEmpty = [](GuestRangeList ranges) { return std::any_of(ranges.begin(), ranges.end(), [](const auto& range) { return range.first < range.second; }); };
        return use.anyReads || use.anyWrites || nonEmpty(use.reads) || nonEmpty(use.writes);
    };
    const bool earlierTouches = anyReads || anyWrites || !reads.Empty() || !writes.Empty();
    if ((access.anyWrites && earlierTouches) || (anyWrites && touchesMemory(access))) return PassHazard::UnknownWrite;
    const bool writesRanges = std::any_of(access.writes.begin(), access.writes.end(), [](const auto& range) { return range.first < range.second; });
    if ((access.anyReads && !writes.Empty()) || (anyReads && writesRanges)) return PassHazard::UnknownRead;
    if (overlapsAny(writes, access.reads)) return PassHazard::ReadAfterWrite;
    if (overlapsAny(writes, access.writes)) return PassHazard::WriteAfterWrite;
    if (overlapsAny(reads, access.writes)) return PassHazard::WriteAfterRead;
    const auto conflicts = [](bool earlier, bool later) { return earlier || later; };
    for (const auto& [image, written] : access.images) {
        if (image == VK_NULL_HANDLE) continue;
        if (std::any_of(images.begin(), images.end(), [&](const auto& seen) { return seen.first == image && conflicts(seen.second, written); })) return PassHazard::Image;
        if (std::any_of(attachments.begin(), attachments.end(), [&](const Attachment& seen) { return seen.image == image && conflicts(seen.written, written); })) return PassHazard::Image;
    }
    for (const auto& [image, written] : access.attachments) {
        if (image == VK_NULL_HANDLE) continue;
        if (std::any_of(images.begin(), images.end(), [&](const auto& seen) { return seen.first == image && conflicts(seen.second, written); })) return PassHazard::Image;
        if (std::any_of(attachments.begin(), attachments.end(), [&](const Attachment& seen) { return seen.image == image && !(samePass && seen.pass == pass) && conflicts(seen.written, written); })) return PassHazard::Image;
    }
    return PassHazard::None;
}

void PassHazards::Add(const PassAccess& access) {
    for (const auto& [begin, end] : access.reads) reads.Insert(begin, end);
    for (const auto& [begin, end] : access.writes) writes.Insert(begin, end);
    for (const auto& [image, written] : access.images) {
        if (image == VK_NULL_HANDLE) continue;
        const auto it = std::find_if(images.begin(), images.end(), [&](const auto& seen) { return seen.first == image; });
        if (it == images.end()) images.emplace_back(image, written);
        else it->second = it->second || written;
    }
    for (const auto& [image, written] : access.attachments) {
        if (image == VK_NULL_HANDLE) continue;
        const auto it = std::find_if(attachments.begin(), attachments.end(), [&](const Attachment& seen) { return seen.image == image && seen.pass == pass; });
        if (it == attachments.end()) attachments.push_back({image, written, pass});
        else it->written = it->written || written;
    }
    anyReads = anyReads || access.anyReads;
    anyWrites = anyWrites || access.anyWrites;
}

void PassHazards::Clear() {
    reads.Clear();
    writes.Clear();
    images.clear();
    attachments.clear();
    anyReads = anyWrites = false;
}

bool PassHazards::Empty() const {
    return reads.Empty() && writes.Empty() && images.empty() && attachments.empty() && !anyReads && !anyWrites;
}

}
