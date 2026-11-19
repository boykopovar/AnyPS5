#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/WrittenBufferIndex.hpp"
#include <algorithm>
#include <utility>

namespace AgcDriver::DriverDetail {

WrittenBufferIndex::WrittenBufferIndex(std::size_t capacity) : capacity(std::max<std::size_t>(capacity, 1)) {}

std::uint64_t WrittenBufferIndex::firstGranule(const WrittenBuffer& buffer) {
    return buffer.begin >> GranuleBits;
}

std::uint64_t WrittenBufferIndex::lastGranule(const WrittenBuffer& buffer) {
    return buffer.end > buffer.begin ? (buffer.end - 1) >> GranuleBits : buffer.begin >> GranuleBits;
}

bool WrittenBufferIndex::wide(const WrittenBuffer& buffer) {
    return lastGranule(buffer) - firstGranule(buffer) >= WideGranules;
}

void WrittenBufferIndex::Push(WrittenBuffer buffer) {
    const auto sequence = evicted + entries.size();
    if (wide(buffer)) {
        wideEntries.push_back(sequence);
    } else {
        for (auto granule = firstGranule(buffer); granule <= lastGranule(buffer); ++granule) granules[granule].push_back(sequence);
    }
    entries.push_back(std::move(buffer));
    while (entries.size() > capacity) evictOldest();
}

void WrittenBufferIndex::evictOldest() {
    const auto& oldest = entries.front();
    if (wide(oldest)) {
        wideEntries.pop_front();
    } else {
        for (auto granule = firstGranule(oldest); granule <= lastGranule(oldest); ++granule) {
            const auto found = granules.find(granule);
            found->second.pop_front();
            if (found->second.empty()) granules.erase(found);
        }
    }
    entries.pop_front();
    ++evicted;
}

std::optional<WrittenBuffer> WrittenBufferIndex::scan(std::uint64_t begin, std::uint64_t end) const {
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        if (begin < it->end && it->begin < end) return *it;
    }
    return std::nullopt;
}

std::optional<WrittenBuffer> WrittenBufferIndex::Newest(std::uint64_t begin, std::uint64_t end) const {
    if (end <= begin || ((end - 1) >> GranuleBits) - (begin >> GranuleBits) >= WideGranules) return scan(begin, end);
    std::optional<std::uint64_t> newest;
    const auto consider = [&](const std::deque<std::uint64_t>& sequences) {
        for (auto it = sequences.rbegin(); it != sequences.rend() && (!newest || *it > *newest); ++it) {
            const auto& entry = entries[static_cast<std::size_t>(*it - evicted)];
            if (begin < entry.end && entry.begin < end) {
                newest = *it;
                return;
            }
        }
    };
    consider(wideEntries);
    for (auto granule = begin >> GranuleBits; granule <= (end - 1) >> GranuleBits; ++granule) {
        if (const auto found = granules.find(granule); found != granules.end()) consider(found->second);
    }
    if (!newest) return std::nullopt;
    return entries[static_cast<std::size_t>(*newest - evicted)];
}

const std::deque<WrittenBuffer>& WrittenBufferIndex::Entries() const {
    return entries;
}

}
