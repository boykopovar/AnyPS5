#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MULTIPLE_MASTERS_H

#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace FreeType;

namespace {

void RequireLp64(const char* function) {
    if (sizeof(FT_Long) != 8) {
        throw std::runtime_error(std::string(function) + ": the LP64 variation layouts need a 64-bit host FT_Long, which is not implemented");
    }
}

std::vector<FT_Fixed> HostCoordinates(const std::int64_t* coordinates, std::uint32_t count, const char* function) {
    std::vector<FT_Fixed> host;
    if (!coordinates) return host;
    host.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        if (coordinates[index] < std::numeric_limits<FT_Fixed>::min() || coordinates[index] > std::numeric_limits<FT_Fixed>::max()) {
            throw std::runtime_error(std::string(function) + ": a coordinate outside the host FT_Fixed range is not implemented");
        }
        host.push_back(static_cast<FT_Fixed>(coordinates[index]));
    }
    return host;
}

void PublishCoordinates(std::int64_t* coordinates, const std::vector<FT_Fixed>& host) {
    GuestArena::HostWrite write(coordinates, host.size() * sizeof(*coordinates));
    for (std::size_t index = 0; index < host.size(); ++index) coordinates[index] = host[index];
}

}

extern "C" {

int APS5_VABI FT_Get_MM_Var_nid_postfix(GuestFaceRec* face, void** master) {
    const GuestFace* owner = FaceOf(face);
    if (owner) RequireLp64(__func__);
    FT_MM_Var* native = nullptr;
    const FT_Error error = FT_Get_MM_Var(owner ? owner->host : nullptr, master ? &native : nullptr);
    if (error != 0 || !master) return error;
    GuestArena::HostWrite write(master, sizeof(*master));
    *master = native;
    return 0;
}

int APS5_VABI FT_Done_MM_Var_nid_postfix(GuestLibrary* library, void* master) {
    return FT_Done_MM_Var(library ? library->host : nullptr, static_cast<FT_MM_Var*>(master));
}

int APS5_VABI FT_Set_Var_Design_Coordinates_nid_postfix(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    GuestFace* owner = FaceOf(face);
    std::vector<FT_Fixed> host = HostCoordinates(coordinates, count, __func__);
    FT_Fixed placeholder = 0;
    const FT_Error error = FT_Set_Var_Design_Coordinates(owner ? owner->host : nullptr, count, coordinates ? (host.empty() ? &placeholder : host.data()) : nullptr);
    if (owner) SyncFace(*owner);
    return error;
}

int APS5_VABI FT_Get_Var_Design_Coordinates_nid_postfix(GuestFaceRec* face, std::uint32_t count, std::int64_t* coordinates) {
    const GuestFace* owner = FaceOf(face);
    std::vector<FT_Fixed> host(coordinates ? count : 0, 0);
    FT_Fixed placeholder = 0;
    const FT_Error error = FT_Get_Var_Design_Coordinates(owner ? owner->host : nullptr, count, coordinates ? (host.empty() ? &placeholder : host.data()) : nullptr);
    if (error == 0 && coordinates) PublishCoordinates(coordinates, host);
    return error;
}

int APS5_VABI FT_Set_Var_Blend_Coordinates_nid_postfix(GuestFaceRec* face, std::uint32_t count, const std::int64_t* coordinates) {
    GuestFace* owner = FaceOf(face);
    std::vector<FT_Fixed> host = HostCoordinates(coordinates, count, __func__);
    FT_Fixed placeholder = 0;
    const FT_Error error = FT_Set_Var_Blend_Coordinates(owner ? owner->host : nullptr, count, coordinates ? (host.empty() ? &placeholder : host.data()) : nullptr);
    if (owner) SyncFace(*owner);
    return error;
}

}
