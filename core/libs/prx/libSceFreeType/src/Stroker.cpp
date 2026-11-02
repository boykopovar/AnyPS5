#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_STROKER_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using namespace FreeType;

namespace {

FT_Pos HostCoordinate(std::int64_t value, const char* function) {
    if (value < std::numeric_limits<FT_Pos>::min() || value > std::numeric_limits<FT_Pos>::max()) {
        throw std::runtime_error(std::string(function) + ": a value outside the host FT_Pos range is not implemented");
    }
    return static_cast<FT_Pos>(value);
}

struct NativeVector {
    FT_Vector value{};
    FT_Vector* pointer = nullptr;

    NativeVector(const GuestVector* guest, const char* function) {
        if (!guest) return;
        value = {HostCoordinate(guest->x, function), HostCoordinate(guest->y, function)};
        pointer = &value;
    }
};

void WriteCount(std::uint32_t* target, FT_UInt value) {
    if (!target) return;
    GuestArena::HostWrite write(target, sizeof(*target));
    *target = value;
}

}

extern "C" {

int APS5_VABI FT_Stroker_New_nid_postfix(GuestLibrary* library, void** stroker) {
    FT_Stroker native = nullptr;
    const FT_Error error = FT_Stroker_New(library ? library->host : nullptr, stroker ? &native : nullptr);
    if (stroker) *stroker = native;
    return error;
}

void APS5_VABI FT_Stroker_Done_nid_postfix(void* stroker) {
    FT_Stroker_Done(static_cast<FT_Stroker>(stroker));
}

int APS5_VABI FT_Stroker_GetCounts_nid_postfix(void* stroker, std::uint32_t* points, std::uint32_t* contours) {
    FT_UInt nativePoints = 0;
    FT_UInt nativeContours = 0;
    const FT_Error error = FT_Stroker_GetCounts(static_cast<FT_Stroker>(stroker), points ? &nativePoints : nullptr, contours ? &nativeContours : nullptr);
    WriteCount(points, nativePoints);
    WriteCount(contours, nativeContours);
    return error;
}

int APS5_VABI FT_Stroker_BeginSubPath_nid_postfix(void* stroker, const GuestVector* to, std::uint8_t open) {
    NativeVector vector(to, __func__);
    return FT_Stroker_BeginSubPath(static_cast<FT_Stroker>(stroker), vector.pointer, open);
}

int APS5_VABI FT_Stroker_LineTo_nid_postfix(void* stroker, const GuestVector* to) {
    NativeVector vector(to, __func__);
    return FT_Stroker_LineTo(static_cast<FT_Stroker>(stroker), vector.pointer);
}

int APS5_VABI FT_Stroker_EndSubPath_nid_postfix(void* stroker) {
    return FT_Stroker_EndSubPath(static_cast<FT_Stroker>(stroker));
}

void APS5_VABI FT_Stroker_Set_nid_postfix(void* stroker, std::int64_t radius, std::int32_t lineCap, std::int32_t lineJoin, std::int64_t miterLimit) {
    FT_Stroker_Set(static_cast<FT_Stroker>(stroker), HostCoordinate(radius, __func__), static_cast<FT_Stroker_LineCap>(lineCap),
                   static_cast<FT_Stroker_LineJoin>(lineJoin), HostCoordinate(miterLimit, __func__));
}

void APS5_VABI FT_Stroker_Rewind_nid_postfix(void* stroker) {
    FT_Stroker_Rewind(static_cast<FT_Stroker>(stroker));
}

int APS5_VABI FT_Stroker_GetBorderCounts_nid_postfix(void* stroker, std::int32_t border, std::uint32_t* points, std::uint32_t* contours) {
    FT_UInt nativePoints = 0;
    FT_UInt nativeContours = 0;
    const FT_Error error = FT_Stroker_GetBorderCounts(static_cast<FT_Stroker>(stroker), static_cast<FT_StrokerBorder>(border),
                                                      points ? &nativePoints : nullptr, contours ? &nativeContours : nullptr);
    WriteCount(points, nativePoints);
    WriteCount(contours, nativeContours);
    return error;
}

int APS5_VABI FT_Stroker_ConicTo_nid_postfix(void* stroker, const GuestVector* control, const GuestVector* to) {
    NativeVector nativeControl(control, __func__);
    NativeVector nativeTo(to, __func__);
    return FT_Stroker_ConicTo(static_cast<FT_Stroker>(stroker), nativeControl.pointer, nativeTo.pointer);
}

int APS5_VABI FT_Stroker_CubicTo_nid_postfix(void* stroker, const GuestVector* control1, const GuestVector* control2, const GuestVector* to) {
    NativeVector first(control1, __func__);
    NativeVector second(control2, __func__);
    NativeVector end(to, __func__);
    return FT_Stroker_CubicTo(static_cast<FT_Stroker>(stroker), first.pointer, second.pointer, end.pointer);
}

}
