#include "prx/libcairo/include/Region.hpp"
#include "prx/libc/include/General.hpp"

#include <CairoBackend.h>

#include <bit>
#include <cerrno>
#include <cstddef>

static_assert(sizeof(CairoRectangleInt) == sizeof(cairo_rectangle_int_t));
static_assert(offsetof(CairoRectangleInt, y) == offsetof(cairo_rectangle_int_t, y));
static_assert(offsetof(CairoRectangleInt, width) == offsetof(cairo_rectangle_int_t, width));
static_assert(offsetof(CairoRectangleInt, height) == offsetof(cairo_rectangle_int_t, height));

namespace {

cairo_region_t* Backend(CairoRegion* region) {
    return reinterpret_cast<cairo_region_t*>(region);
}

const cairo_region_t* Backend(const CairoRegion* region) {
    return reinterpret_cast<const cairo_region_t*>(region);
}

template <typename T>
T ReadGuest(const T* source, const char* caller) {
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    errno = savedErrno;
    if (!ok)
        NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename T>
void WriteGuest(T* destination, const T& value, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    if (!ok)
        NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

CairoRegion* APS5_VABI cairo_region_create() {
    return reinterpret_cast<CairoRegion*>(aps5_cairo_region_create());
}

CairoRegion* APS5_VABI cairo_region_reference(CairoRegion* region) {
    return reinterpret_cast<CairoRegion*>(aps5_cairo_region_reference(Backend(region)));
}

void APS5_VABI cairo_region_destroy(CairoRegion* region) {
    aps5_cairo_region_destroy(Backend(region));
}

int APS5_VABI cairo_region_union_rectangle(CairoRegion* region, const CairoRectangleInt* rectangle) {
    if (region == nullptr || rectangle == nullptr)
        return CAIRO_STATUS_NULL_POINTER;
    const CairoRectangleInt local = ReadGuest(rectangle, __func__);
    const cairo_rectangle_int_t added = std::bit_cast<cairo_rectangle_int_t>(local);
    return aps5_cairo_region_union_rectangle(Backend(region), &added);
}

void APS5_VABI cairo_region_get_rectangle(const CairoRegion* region, int nth, CairoRectangleInt* rectangle) {
    if (rectangle == nullptr)
        return;
    CairoRectangleInt output{};
    if (region != nullptr && nth >= 0 && nth < aps5_cairo_region_num_rectangles(Backend(region))) {
        cairo_rectangle_int_t result;
        aps5_cairo_region_get_rectangle(Backend(region), nth, &result);
        output = std::bit_cast<CairoRectangleInt>(result);
    }
    WriteGuest(rectangle, output, __func__);
}

int APS5_VABI cairo_region_num_rectangles(const CairoRegion* region) {
    if (region == nullptr)
        return 0;
    return aps5_cairo_region_num_rectangles(Backend(region));
}

}
