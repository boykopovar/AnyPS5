#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUARDSPANS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUARDSPANS_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

class GuardSpans {
public:
    std::uint64_t Generation() const { return generation; }

    template<typename TRanges>
    void Fill(std::uint64_t filled, const TRanges& ranges) {
        spans.clear();
        for (const auto& range : ranges) spans.emplace_back(range.address, range.address + range.bytes);
        std::ranges::sort(spans);
        std::uint64_t reach = 0;
        for (auto& span : spans) span.second = reach = std::max(reach, span.second);
        generation = filled;
    }

    bool Overlaps(std::uint64_t address, std::uint64_t bytes) const {
        const auto end = address + bytes;
        const auto after = std::ranges::lower_bound(spans, end, {}, &std::pair<std::uint64_t, std::uint64_t>::first);
        return after != spans.begin() && std::prev(after)->second > address;
    }

private:
    std::uint64_t generation = std::numeric_limits<std::uint64_t>::max();
    std::vector<std::pair<std::uint64_t, std::uint64_t>> spans;
};

enum class GuardSpanEvent : std::size_t { Clear, Refill, Overlap, Verified, Missed, Count };

void NoteGuardSpans(GuardSpanEvent event);
bool VerifyGuardSpans();
void ReportGuardSpanMiss(const char* list, std::uint64_t address, std::uint64_t bytes);

}

#endif
