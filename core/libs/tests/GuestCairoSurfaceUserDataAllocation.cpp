#include "prx/libcairo/include/Surface.hpp"

#include <CairoBackend.h>

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstddef>
#include <new>

static bool rejectAllocation{};
static unsigned rejectedAllocations{};

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (rejectAllocation) {
        ++rejectedAllocations;
        return nullptr;
    }
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}

namespace {

CairoSurface* finishingSurface{};
CairoUserDataKey key{};
int data{};
unsigned callbacks{};

void APS5_VABI Reenter(void* pointer) {
    assert(pointer == &data);
    ++callbacks;
    const unsigned before = rejectedAllocations;
    rejectAllocation = true;
    assert(cairo_surface_set_user_data(finishingSurface, &key, &data, Reenter) == CAIRO_STATUS_SURFACE_FINISHED);
    assert(rejectedAllocations == before);
    rejectAllocation = false;
}

std::array<void*, 4> events{};
std::size_t eventCount{};

void APS5_VABI Record(void* pointer) {
    assert(eventCount < events.size());
    events[eventCount++] = pointer;
}

void CheckReplacementOom() {
    auto* surface = cairo_image_surface_create(0, 1, 1);
    assert(surface && cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS);
    CairoUserDataKey replacementKey{};
    int first{};
    int second{};

    assert(cairo_surface_set_user_data(surface, &replacementKey, &first, Record) == CAIRO_STATUS_SUCCESS);
    assert(eventCount == 0);

    const unsigned before = rejectedAllocations;
    rejectAllocation = true;
    assert(cairo_surface_set_user_data(surface, &replacementKey, &second, Record) == CAIRO_STATUS_NO_MEMORY);
    rejectAllocation = false;
    assert(rejectedAllocations == before + 1);
    assert(eventCount == 0);

    assert(cairo_surface_set_user_data(surface, &replacementKey, &second, Record) == CAIRO_STATUS_SUCCESS);
    assert(eventCount == 1);
    assert(events[0] == &first);

    cairo_surface_destroy(surface);
    assert(eventCount == 2);
    assert(events[1] == &second);
}

}

int main() {
    auto* invalid = cairo_image_surface_create(-1, 1, 1);
    rejectAllocation = true;
    assert(cairo_surface_set_user_data(invalid, &key, &data, Reenter) == CAIRO_STATUS_INVALID_FORMAT);
    assert(rejectedAllocations == 0);
    rejectAllocation = false;
    cairo_surface_destroy(invalid);

    finishingSurface = cairo_image_surface_create(0, 1, 1);
    rejectAllocation = true;
    assert(cairo_surface_set_user_data(finishingSurface, &key, &data, Reenter) == CAIRO_STATUS_NO_MEMORY);
    assert(rejectedAllocations == 1 && callbacks == 0);
    rejectAllocation = false;
    auto* backend = reinterpret_cast<cairo_surface_t*>(finishingSurface);
    aps5_cairo_surface_finish(backend);
    cairo_surface_mark_dirty_rectangle(finishingSurface, 0, 0, 1, 1);
    assert(cairo_surface_status(finishingSurface) == CAIRO_STATUS_SURFACE_FINISHED);
    assert(cairo_surface_set_user_data(finishingSurface, &key, &data, Reenter) == CAIRO_STATUS_SUCCESS);
    cairo_surface_destroy(finishingSurface);
    assert(callbacks == 1);

    CheckReplacementOom();
}
