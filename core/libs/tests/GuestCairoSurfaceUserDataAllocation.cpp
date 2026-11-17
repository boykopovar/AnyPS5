#include "prx/libcairo/include/Surface.hpp"

#include <CairoBackend.h>

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
}
