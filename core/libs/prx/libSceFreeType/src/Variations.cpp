#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MULTIPLE_MASTERS_H

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace FreeType;

namespace {

bool CopyInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

FT_Library ReadGuestLibrary(GuestLibrary* library, const char* func) {
    if (!library) return nullptr;
    FT_Library host = nullptr;
    if (reinterpret_cast<std::uintptr_t>(library) % alignof(GuestLibrary) != 0 || !CopyInPreservingErrno(&host, &library->host, sizeof(host))) NotImplemented_nid_no_patch(func);
    return host;
}

void RequireLp64(const char* function) {
    if (sizeof(FT_Long) != 8) {
        throw std::runtime_error(std::string(function) + ": the LP64 variation layouts need a 64-bit host FT_Long, which is not implemented");
    }
}

std::vector<FT_Fixed> HostCoordinates(const std::int64_t* coordinates, std::uint32_t count, const char* function) {
    std::vector<FT_Fixed> host;
    if (!coordinates) return host;
    std::vector<std::int64_t> guest(count);
    if (count != 0 && !GuestCopyIn_nid_no_patch(guest.data(), coordinates, count * sizeof(std::int64_t))) {
        throw std::runtime_error(std::string(function) + ": the guest coordinate buffer is not readable, which is not implemented");
    }
    host.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        if (guest[index] < std::numeric_limits<FT_Fixed>::min() || guest[index] > std::numeric_limits<FT_Fixed>::max()) {
            throw std::runtime_error(std::string(function) + ": a coordinate outside the host FT_Fixed range is not implemented");
        }
        host.push_back(static_cast<FT_Fixed>(guest[index]));
    }
    return host;
}

bool PublishCoordinates(std::int64_t* coordinates, const std::vector<FT_Fixed>& host) {
    std::vector<std::int64_t> wide(host.begin(), host.end());
    return wide.empty() || GuestCopyOut_nid_no_patch(coordinates, wide.data(), wide.size() * sizeof(std::int64_t));
}

}

extern "C" {

int APS5_VABI FT_Get_MM_Var_nid_postfix(GuestFaceRec* face, void** master) {
    const GuestFace* owner = FaceOf(face);
    if (master && owner && owner->host && FT_HAS_MULTIPLE_MASTERS(owner->host)) RequireLp64(__func__);
    FT_MM_Var* native = nullptr;
    const FT_Error error = FT_Get_MM_Var(owner ? owner->host : nullptr, master ? &native : nullptr);
    if (error != 0 || !master) return error;
    if (!GuestCopyOut_nid_no_patch(master, &native, sizeof(native))) return FT_Err_Invalid_Argument;
    return 0;
}

int APS5_VABI FT_Done_MM_Var_nid_postfix(GuestLibrary* library, void* master) {
    return FT_Done_MM_Var(ReadGuestLibrary(library, __func__), static_cast<FT_MM_Var*>(master));
}

int APS5_VABI FreeTypeSetVarDesignCoordinates_nid_no_patch(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    GuestFace* owner = FaceOf(face);
    if (!owner) return FT_Err_Invalid_Face_Handle;
    std::vector<FT_Fixed> host = HostCoordinates(coordinates, count, "FT_Set_Var_Design_Coordinates_nid_postfix");
    FT_Fixed placeholder = 0;
    const FT_Error error = FT_Set_Var_Design_Coordinates(owner ? owner->host : nullptr, count, coordinates ? (host.empty() ? &placeholder : host.data()) : nullptr);
    if (owner) SyncFace(*owner);
    return error;
}

int APS5_VABI FT_Set_Var_Design_Coordinates_nid_postfix(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    return FreeTypeSetVarDesignCoordinates_nid_no_patch(face, count, coordinates);
}

int APS5_VABI FT_Get_Var_Design_Coordinates_nid_postfix(GuestFaceRec* face, std::uint32_t count, std::int64_t* coordinates) {
    const GuestFace* owner = FaceOf(face);
    std::vector<FT_Fixed> host(coordinates ? count : 0, 0);
    FT_Fixed placeholder = 0;
    const FT_Error error = FT_Get_Var_Design_Coordinates(owner ? owner->host : nullptr, count, coordinates ? (host.empty() ? &placeholder : host.data()) : nullptr);
    if (error == 0 && coordinates && !PublishCoordinates(coordinates, host)) return FT_Err_Invalid_Argument;
    return error;
}

int APS5_VABI FreeTypeSetVarBlendCoordinates_nid_no_patch(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    GuestFace* owner = FaceOf(face);
    std::vector<FT_Fixed> host = HostCoordinates(coordinates, count, "FT_Set_Var_Blend_Coordinates_nid_postfix");
    FT_Fixed placeholder = 0;
    const FT_Error error = FT_Set_Var_Blend_Coordinates(owner ? owner->host : nullptr, count, coordinates ? (host.empty() ? &placeholder : host.data()) : nullptr);
    if (owner) SyncFace(*owner);
    return error;
}

int APS5_VABI FT_Set_Var_Blend_Coordinates_nid_postfix(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    return FreeTypeSetVarBlendCoordinates_nid_no_patch(face, count, coordinates);
}

int APS5_VABI FT_Get_Var_Blend_Coordinates_nid_postfix(GuestFaceRec* face, std::uint32_t count, std::int64_t* coordinates) {
    const GuestFace* owner = FaceOf(face);
    std::vector<FT_Fixed> host(coordinates ? count : 0, 0);
    FT_Fixed placeholder = 0;
    const FT_Error error = FT_Get_Var_Blend_Coordinates(owner ? owner->host : nullptr, count, coordinates ? (host.empty() ? &placeholder : host.data()) : nullptr);
    if (error == 0 && coordinates && !PublishCoordinates(coordinates, host)) return FT_Err_Invalid_Argument;
    return error;
}

int APS5_VABI FreeTypeSetMMBlendCoordinates_nid_no_patch(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    GuestFace* owner = FaceOf(face);
    std::vector<FT_Fixed> host = HostCoordinates(coordinates, count, "FT_Set_MM_Blend_Coordinates_nid_postfix");
    FT_Fixed placeholder = 0;
    const FT_Error error = FT_Set_MM_Blend_Coordinates(owner ? owner->host : nullptr, count, coordinates ? (host.empty() ? &placeholder : host.data()) : nullptr);
    if (owner) SyncFace(*owner);
    return error;
}

int APS5_VABI FT_Set_MM_Blend_Coordinates_nid_postfix(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    return FreeTypeSetMMBlendCoordinates_nid_no_patch(face, count, coordinates);
}

int APS5_VABI FreeTypeSetMMDesignCoordinates_nid_no_patch(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    GuestFace* owner = FaceOf(face);
    std::vector<FT_Fixed> host = HostCoordinates(coordinates, count, "FT_Set_MM_Design_Coordinates_nid_postfix");
    FT_Fixed placeholder = 0;
    const FT_Error error = FT_Set_MM_Design_Coordinates(owner ? owner->host : nullptr, count, coordinates ? reinterpret_cast<FT_Long*>(host.empty() ? &placeholder : host.data()) : nullptr);
    if (owner) SyncFace(*owner);
    return error;
}

int APS5_VABI FT_Set_MM_Design_Coordinates_nid_postfix(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    return FreeTypeSetMMDesignCoordinates_nid_no_patch(face, count, coordinates);
}

int APS5_VABI FT_Get_Multi_Master_nid_postfix(GuestFaceRec* face, void* master) {
    const GuestFace* owner = FaceOf(face);
    if (master && owner && owner->host && FT_HAS_MULTIPLE_MASTERS(owner->host)) RequireLp64(__func__);
    FT_Multi_Master native{};
    const FT_Error error = FT_Get_Multi_Master(owner ? owner->host : nullptr, master ? &native : nullptr);
    if (error != 0 || !master) return error;
    if (!GuestCopyOut_nid_no_patch(master, &native, sizeof(native))) return FT_Err_Invalid_Argument;
    return 0;
}

}
