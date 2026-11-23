#pragma once

#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace ExtendedPrecisionWords {

template <typename T>
class GuestWords {
public:
    explicit GuestWords(const char* function) : function(function) {}

    std::size_t Add(const T* guest, std::size_t count, bool writes) {
        spans.push_back({reinterpret_cast<std::uintptr_t>(guest), count, writes, 0});
        return spans.size() - 1;
    }

    void Load() {
        std::vector<std::size_t> order;
        for (std::size_t index = 0; index < spans.size(); ++index) {
            if (spans[index].count != 0) order.push_back(index);
        }
        std::sort(order.begin(), order.end(), [this](std::size_t left, std::size_t right) { return spans[left].start < spans[right].start; });
        for (const std::size_t index : order) {
            Span& span = spans[index];
            if (span.start == 0 || span.count > (UINTPTR_MAX - span.start) / sizeof(T)) Fail("unreadable");
            const std::uintptr_t end = span.start + span.count * sizeof(T);
            if (regions.empty() || regions.back().end <= span.start) regions.push_back({span.start, end, false, {}});
            Region& region = regions.back();
            region.end = std::max(region.end, end);
            region.writes = region.writes || span.writes;
            span.region = regions.size() - 1;
        }
        for (Region& region : regions) {
            const std::size_t bytes = region.end - region.start;
            region.words.resize((bytes + sizeof(T) - 1) / sizeof(T));
            if (!GuestCopyIn_nid_no_patch(region.words.data(), reinterpret_cast<const void*>(region.start), bytes)) Fail("unreadable");
            if (region.writes && !GuestWritable_nid_no_patch(reinterpret_cast<void*>(region.start), bytes)) Fail("unwritable");
        }
    }

    T* Host(std::size_t index) {
        const Span& span = spans[index];
        if (span.count == 0) return nullptr;
        Region& region = regions[span.region];
        return reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(region.words.data()) + (span.start - region.start));
    }

    void Store() const {
        for (const Region& region : regions) {
            if (region.writes && !GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(region.start), region.words.data(), region.end - region.start)) {
                Fail("unwritable");
            }
        }
    }

private:
    struct Span {
        std::uintptr_t start;
        std::size_t count;
        bool writes;
        std::size_t region;
    };

    struct Region {
        std::uintptr_t start;
        std::uintptr_t end;
        bool writes;
        std::vector<T> words;
    };

    [[noreturn]] void Fail(const char* what) const {
        throw std::invalid_argument(std::string(function) + ": " + what + " words");
    }

    const char* function;
    std::vector<Span> spans;
    std::vector<Region> regions;
};

template <typename T>
T ReadWord(const T* p, std::size_t index, const char* function) {
    T word{};
    if (!GuestCopyIn_nid_no_patch(&word, p + index, sizeof(word))) throw std::invalid_argument(std::string(function) + ": unreadable words");
    return word;
}

}
