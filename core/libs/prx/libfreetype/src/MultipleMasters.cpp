#include "prx/libfreetype/include/MultipleMasters.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include FT_MULTIPLE_MASTERS_H
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <limits>
#include <vector>

namespace {

constexpr std::uint32_t ScratchLimit = 0x10000;

FT_Face HostFace(const FreeType::GuestFaceRec* face) {
    const auto* owner = face ? static_cast<const FreeType::GuestFace*>(face->internal) : nullptr;
    return owner ? owner->host : nullptr;
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool WriteValues(std::int64_t* destination, std::uint32_t count, const std::vector<FT_Fixed>& native, std::uint32_t used) {
    std::array<std::int64_t, 4096> chunk;
    for (std::uint32_t base = 0; base < count;) {
        const std::uint32_t size = std::min<std::uint32_t>(static_cast<std::uint32_t>(chunk.size()), count - base);
        for (std::uint32_t index = 0; index < size; ++index) chunk[index] = native[std::min(base + index, used - 1)];
        if (!CopyOut(destination + base, chunk.data(), size * sizeof(std::int64_t))) return false;
        base += size;
    }
    return true;
}

std::int32_t Synced(FreeType::GuestFaceRec* face, FT_Error error) {
    FreeTypeSyncFace_nid_no_patch(face);
    return error;
}

}

extern "C" {

std::int32_t APS5_VABI FT_Get_MM_Blend_Coordinates_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t numCoords, std::int64_t* coords) {
    const std::uint32_t used = std::min(numCoords, ScratchLimit);
    std::vector<FT_Fixed> native(coords ? static_cast<std::size_t>(used) + 1 : 0);
    const FT_Error error = FT_Get_MM_Blend_Coordinates(HostFace(face), used, coords ? native.data() : nullptr);
    if (error == FT_Err_Ok && !WriteValues(coords, numCoords, native, used)) return FT_Err_Invalid_Argument;
    return error;
}

std::int32_t APS5_VABI FT_Get_MM_WeightVector_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t* length, std::int64_t* weightVector) {
    FT_UInt requested = 0;
    if (length && !CopyIn(&requested, length, sizeof(requested))) return FT_Err_Invalid_Argument;
    const FT_UInt used = std::min<FT_UInt>(requested, ScratchLimit);
    FT_UInt nativeLength = used;
    std::vector<FT_Fixed> native(weightVector ? static_cast<std::size_t>(used) + 1 : 0);
    const FT_Error error = FT_Get_MM_WeightVector(HostFace(face), length ? &nativeLength : nullptr, weightVector ? native.data() : nullptr);
    const FT_UInt reported = nativeLength == used ? requested : nativeLength;
    if (length && !CopyOut(length, &reported, sizeof(reported))) return FT_Err_Invalid_Argument;
    if (error == FT_Err_Ok && !WriteValues(weightVector, requested, native, used)) return FT_Err_Invalid_Argument;
    return error;
}

std::int32_t APS5_VABI FT_Set_MM_WeightVector_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t length, std::int64_t* weightVector) {
    const FT_Face host = HostFace(face);
    if (!weightVector) return Synced(face, FT_Set_MM_WeightVector(host, length, nullptr));
    FT_UInt designs = 0;
    FT_Fixed probe = 0;
    FT_Get_MM_WeightVector(host, &designs, &probe);
    const FT_UInt used = std::min<FT_UInt>(length, designs);
    std::vector<std::int64_t> requested(used);
    if (used && !CopyIn(requested.data(), weightVector, used * sizeof(std::int64_t))) return FT_Err_Invalid_Argument;
    std::vector<FT_Fixed> native(static_cast<std::size_t>(used) + 1);
    for (FT_UInt index = 0; index < used; ++index) {
        if (requested[index] < std::numeric_limits<FT_Fixed>::min() || requested[index] > std::numeric_limits<FT_Fixed>::max()) return FT_Err_Invalid_Argument;
        native[index] = static_cast<FT_Fixed>(requested[index]);
    }
    return Synced(face, FT_Set_MM_WeightVector(host, used, native.data()));
}

std::int32_t APS5_VABI FT_Set_Named_Instance_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t instanceIndex) {
    return Synced(face, FT_Set_Named_Instance(HostFace(face), instanceIndex));
}

}
