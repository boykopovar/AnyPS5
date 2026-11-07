#include "prx/libfreetype/include/MultipleMasters.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include FT_MULTIPLE_MASTERS_H
#include <algorithm>
#include <vector>

namespace {

constexpr std::uint32_t ScratchLimit = 0x10000;

FT_Face HostFace(const FreeType::GuestFaceRec* face) {
    const auto* owner = face ? static_cast<const FreeType::GuestFace*>(face->internal) : nullptr;
    return owner ? owner->host : nullptr;
}

}

extern "C" {

std::int32_t APS5_VABI FT_Get_MM_Blend_Coordinates_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t numCoords, std::int64_t* coords) {
    const std::uint32_t used = std::min(numCoords, ScratchLimit);
    std::vector<FT_Fixed> native(coords ? static_cast<std::size_t>(used) + 1 : 0);
    const FT_Error error = FT_Get_MM_Blend_Coordinates(HostFace(face), used, coords ? native.data() : nullptr);
    if (error == FT_Err_Ok) {
        for (std::uint32_t index = 0; index < numCoords; ++index) coords[index] = native[std::min(index, used - 1)];
    }
    return error;
}

std::int32_t APS5_VABI FT_Get_MM_WeightVector_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t* length, std::int64_t* weightVector) {
    const FT_UInt requested = length ? *length : 0;
    const FT_UInt used = std::min<FT_UInt>(requested, ScratchLimit);
    FT_UInt nativeLength = used;
    std::vector<FT_Fixed> native(weightVector ? static_cast<std::size_t>(used) + 1 : 0);
    const FT_Error error = FT_Get_MM_WeightVector(HostFace(face), length ? &nativeLength : nullptr, weightVector ? native.data() : nullptr);
    if (length) *length = nativeLength == used ? requested : nativeLength;
    if (error == FT_Err_Ok) {
        for (FT_UInt index = 0; index < requested; ++index) weightVector[index] = native[std::min(index, used - 1)];
    }
    return error;
}

}
