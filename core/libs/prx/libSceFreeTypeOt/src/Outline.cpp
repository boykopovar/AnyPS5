#include "prx/libSceFreeTypeOt/include/Outline.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>

namespace {

constexpr int InvalidArgument = 0x06;
constexpr std::int32_t OutlineOwner = 0x1;

template <typename T>
T* NewArray(FreeType::GuestMemoryRec* memory, std::int64_t count, std::int32_t* error) {
    return static_cast<T*>(FreeType::OtMemRealloc(memory, static_cast<std::int64_t>(sizeof(T)), 0, count, nullptr, error));
}

}

extern "C" {

int APS5_VABI FT_Outline_Done_Internal_nid_postfix(FreeType::GuestMemoryRec* memory, FreeType::GuestOutline* outline) {
    if (!memory || !outline) return InvalidArgument;
    FreeType::GuestOutline localOutline{};
    if (!GuestCopyIn_nid_no_patch(&localOutline, outline, sizeof(localOutline)) ||
        !GuestWritable_nid_no_patch(outline, sizeof(*outline))) {
        return InvalidArgument;
    }
    if (localOutline.flags & OutlineOwner) {
        FreeType::OtMemFree(memory, localOutline.points);
        FreeType::OtMemFree(memory, localOutline.tags);
        FreeType::OtMemFree(memory, localOutline.contours);
    }
    localOutline = {};
    if (!GuestCopyOut_nid_no_patch(outline, &localOutline, sizeof(localOutline))) return InvalidArgument;
    return 0;
}

int APS5_VABI FT_Outline_New_Internal_nid_postfix(FreeType::GuestMemoryRec* memory, std::uint32_t numPoints, std::int32_t numContours, FreeType::GuestOutline* anoutline) {
    if (!anoutline || !memory) return InvalidArgument;
    if (!GuestWritable_nid_no_patch(anoutline, sizeof(*anoutline))) return InvalidArgument;
    FreeType::GuestOutline localOutline{};
    std::int32_t error = 0;
    try {
        localOutline.points = NewArray<FreeType::GuestVector>(memory, numPoints, &error);
        if (!error) localOutline.tags = NewArray<unsigned char>(memory, numPoints, &error);
        if (!error) localOutline.contours = NewArray<std::uint16_t>(memory, numContours, &error);
    } catch (...) {
        FreeType::OtMemFree(memory, localOutline.points);
        FreeType::OtMemFree(memory, localOutline.tags);
        const FreeType::GuestOutline empty{};
        GuestCopyOut_nid_no_patch(anoutline, &empty, sizeof(empty));
        throw;
    }
    localOutline.flags |= OutlineOwner;
    if (error) {
        if (localOutline.flags & OutlineOwner) {
            FreeType::OtMemFree(memory, localOutline.points);
            FreeType::OtMemFree(memory, localOutline.tags);
            FreeType::OtMemFree(memory, localOutline.contours);
        }
        localOutline = {};
        GuestCopyOut_nid_no_patch(anoutline, &localOutline, sizeof(localOutline));
        return error;
    }
    localOutline.n_points = static_cast<std::uint16_t>(numPoints);
    localOutline.n_contours = static_cast<std::uint16_t>(numContours);
    if (!GuestCopyOut_nid_no_patch(anoutline, &localOutline, sizeof(localOutline))) {
        FreeType::OtMemFree(memory, localOutline.points);
        FreeType::OtMemFree(memory, localOutline.tags);
        FreeType::OtMemFree(memory, localOutline.contours);
        return InvalidArgument;
    }
    return 0;
}

}
