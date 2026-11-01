#include "prx/libcairo/include/InventoryExports.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

struct CairoBox {
    std::int32_t x1;
    std::int32_t y1;
    std::int32_t x2;
    std::int32_t y2;
};

struct CairoRegionStorage {
    std::atomic<unsigned int> refCount{1};
    int status;
    std::vector<CairoBox> rectangles;

    explicit CairoRegionStorage(int state = 0) : status(state) {}
};

namespace {

constexpr int success = 0;
constexpr int noMemory = 1;
constexpr int nullPointer = 7;
constexpr int invalidSize = 32;

CairoRegion* ErrorRegion() {
    static CairoRegionStorage region(noMemory);
    return reinterpret_cast<CairoRegion*>(&region);
}

CairoRegionStorage* Storage(CairoRegion* region) {
    return reinterpret_cast<CairoRegionStorage*>(region);
}

const CairoRegionStorage* Storage(const CairoRegion* region) {
    return reinterpret_cast<const CairoRegionStorage*>(region);
}

void AppendBand(std::vector<CairoBox>& result, std::size_t& previousStart,
                std::size_t& previousCount, std::vector<CairoBox>& intervals,
                std::int32_t top, std::int32_t bottom) {
    if (intervals.empty()) {
        previousCount = 0;
        return;
    }

    std::sort(intervals.begin(), intervals.end(), [](const CairoBox& a, const CairoBox& b) {
        return a.x1 < b.x1 || (a.x1 == b.x1 && a.x2 < b.x2);
    });

    std::size_t count = 0;
    for (const auto& interval : intervals) {
        if (count && intervals[count - 1].x2 >= interval.x1) {
            intervals[count - 1].x2 = std::max(intervals[count - 1].x2, interval.x2);
        } else {
            intervals[count++] = interval;
        }
    }

    bool coalesce = previousCount == count && previousCount != 0;
    for (std::size_t i = 0; coalesce && i < count; ++i) {
        const auto& previous = result[previousStart + i];
        coalesce = previous.y2 == top && previous.x1 == intervals[i].x1 && previous.x2 == intervals[i].x2;
    }

    if (coalesce) {
        for (std::size_t i = 0; i < count; ++i)
            result[previousStart + i].y2 = bottom;
        return;
    }

    previousStart = result.size();
    previousCount = count;
    for (std::size_t i = 0; i < count; ++i)
        result.push_back({intervals[i].x1, top, intervals[i].x2, bottom});
}

std::vector<CairoBox> UnionRectangle(const std::vector<CairoBox>& source, const CairoBox& added) {
    std::vector<std::int32_t> boundaries;
    boundaries.reserve(source.size() * 2 + 2);
    for (const auto& box : source) {
        boundaries.push_back(box.y1);
        boundaries.push_back(box.y2);
    }
    boundaries.push_back(added.y1);
    boundaries.push_back(added.y2);
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    std::vector<CairoBox> result;
    result.reserve(source.size() + boundaries.size());
    std::vector<CairoBox> intervals;
    std::size_t cursor = 0;
    std::size_t previousStart = 0;
    std::size_t previousCount = 0;

    for (std::size_t band = 1; band < boundaries.size(); ++band) {
        const auto top = boundaries[band - 1];
        const auto bottom = boundaries[band];
        intervals.clear();
        while (cursor < source.size() && source[cursor].y2 <= top)
            ++cursor;
        for (std::size_t i = cursor; i < source.size() && source[i].y1 <= top; ++i) {
            if (source[i].y2 >= bottom)
                intervals.push_back(source[i]);
        }
        if (top >= added.y1 && bottom <= added.y2)
            intervals.push_back(added);
        AppendBand(result, previousStart, previousCount, intervals, top, bottom);
    }
    return result;
}

[[gnu::noinline]] int UnionSafely(CairoRegionStorage* region, const CairoBox& added) {
    try {
        auto merged = UnionRectangle(region->rectangles, added);
        if (merged.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return region->status = noMemory;
        for (const auto& box : merged) {
            if (std::int64_t{box.x2} - box.x1 > std::numeric_limits<int>::max() ||
                std::int64_t{box.y2} - box.y1 > std::numeric_limits<int>::max())
                return region->status = invalidSize;
        }
        region->rectangles.swap(merged);
    } catch (const std::bad_alloc&) {
        return region->status = noMemory;
    } catch (const std::length_error&) {
        return region->status = noMemory;
    }
    return success;
}

}

extern "C" {

CairoRegion* APS5_VABI cairo_region_create() {
    auto* region = new (std::nothrow) CairoRegionStorage;
    return region ? reinterpret_cast<CairoRegion*>(region) : ErrorRegion();
}

CairoRegion* APS5_VABI cairo_region_reference(CairoRegion* region) {
    if (region == nullptr || region == ErrorRegion())
        return region;
    Storage(region)->refCount.fetch_add(1, std::memory_order_relaxed);
    return region;
}

void APS5_VABI cairo_region_destroy(CairoRegion* region) {
    if (region == nullptr || region == ErrorRegion())
        return;
    if (Storage(region)->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete Storage(region);
}

int APS5_VABI cairo_region_union_rectangle(CairoRegion* region, const CairoRectangleInt* rectangle) {
    if (region == nullptr || rectangle == nullptr)
        return nullPointer;
    auto* storage = Storage(region);
    if (storage->status != success)
        return storage->status;
    if (rectangle->width <= 0 || rectangle->height <= 0)
        return success;

    const auto x2 = std::int64_t{rectangle->x} + rectangle->width;
    const auto y2 = std::int64_t{rectangle->y} + rectangle->height;
    if (x2 > std::numeric_limits<std::int32_t>::max() || y2 > std::numeric_limits<std::int32_t>::max())
        return storage->status = invalidSize;

    const CairoBox added{rectangle->x, rectangle->y, static_cast<std::int32_t>(x2), static_cast<std::int32_t>(y2)};
    return UnionSafely(storage, added);
}

void APS5_VABI cairo_region_get_rectangle(const CairoRegion* region, int nth, CairoRectangleInt* rectangle) {
    if (rectangle == nullptr)
        return;
    *rectangle = {};
    if (region == nullptr || Storage(region)->status != success || nth < 0 ||
        static_cast<std::size_t>(nth) >= Storage(region)->rectangles.size())
        return;
    const auto& box = Storage(region)->rectangles[static_cast<std::size_t>(nth)];
    rectangle->x = box.x1;
    rectangle->y = box.y1;
    rectangle->width = static_cast<int>(std::int64_t{box.x2} - box.x1);
    rectangle->height = static_cast<int>(std::int64_t{box.y2} - box.y1);
}

int APS5_VABI cairo_region_num_rectangles(const CairoRegion* region) {
    if (region == nullptr || Storage(region)->status != success)
        return 0;
    return static_cast<int>(Storage(region)->rectangles.size());
}

}
