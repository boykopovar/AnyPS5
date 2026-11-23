#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MULTIPLE_MASTERS_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

using namespace FreeType;

namespace {

FT_Library ReadGuestLibrary(GuestLibrary* library, const char* func) {
    if (!library) return nullptr;
    FT_Library host = nullptr;
    if (reinterpret_cast<std::uintptr_t>(library) % alignof(GuestLibrary) != 0 || !GuestCopyIn_nid_no_patch(&host, &library->host, sizeof(host))) NotImplemented_nid_no_patch(func);
    return host;
}

struct VariationAxis64 {
    char* name;
    std::int64_t minimum;
    std::int64_t def;
    std::int64_t maximum;
    std::uint64_t tag;
    std::uint32_t strid;
};

struct NamedStyle64 {
    std::int64_t* coordinates;
    std::uint32_t strid;
    std::uint32_t psid;
};

static_assert(sizeof(FT_MM_Var) == 32 && offsetof(FT_MM_Var, axis) == 16);
static_assert(sizeof(VariationAxis64) == 48 && sizeof(NamedStyle64) == 16);
static_assert(sizeof(std::size_t) == 8 && sizeof(FT_UInt) == 4 && sizeof(FT_UShort) == 2);

std::unique_ptr<std::byte[]> PackMMVar(const FT_MM_Var& native) {
    const std::size_t numAxes = native.num_axis;
    const std::size_t numStyles = native.num_namedstyles;
    const std::size_t flagBytes = numAxes * sizeof(FT_UShort);
    constexpr std::size_t alignment = alignof(VariationAxis64);
    const std::size_t axisOffset = (sizeof(FT_MM_Var) + flagBytes + alignment - 1) & ~(alignment - 1);
    const std::size_t styleOffset = axisOffset + numAxes * sizeof(VariationAxis64);
    const std::size_t coordinateOffset = styleOffset + numStyles * sizeof(NamedStyle64);
    const std::size_t coordinateCount = numAxes * numStyles;
    constexpr auto limit = std::numeric_limits<std::size_t>::max();
    if (coordinateCount > (limit - coordinateOffset) / sizeof(std::int64_t)) return {};
    const std::size_t nameOffset = coordinateOffset + coordinateCount * sizeof(std::int64_t);

    std::size_t size = nameOffset;
    for (std::size_t index = 0; index < numAxes; ++index) {
        if (const char* name = native.axis[index].name) {
            const std::size_t length = std::strlen(name);
            if (length >= limit - size) return {};
            size += length + 1;
        }
    }

    auto block = std::unique_ptr<std::byte[]>(new (std::nothrow) std::byte[size]);
    if (!block) return {};
    auto* axes = reinterpret_cast<VariationAxis64*>(block.get() + axisOffset);
    auto* styles = reinterpret_cast<NamedStyle64*>(block.get() + styleOffset);
    auto* coordinates = reinterpret_cast<std::int64_t*>(block.get() + coordinateOffset);
    auto* names = reinterpret_cast<char*>(block.get() + nameOffset);
    auto* result = reinterpret_cast<FT_MM_Var*>(block.get());
    *result = {native.num_axis, native.num_designs, native.num_namedstyles,
               native.axis ? reinterpret_cast<FT_Var_Axis*>(axes) : nullptr,
               native.namedstyle ? reinterpret_cast<FT_Var_Named_Style*>(styles) : nullptr};
    if (flagBytes != 0) std::memcpy(block.get() + sizeof(FT_MM_Var), &native + 1, flagBytes);

    for (std::size_t index = 0; index < numAxes; ++index) {
        const auto& axis = native.axis[index];
        char* name = nullptr;
        if (const char* source = axis.name) {
            name = names;
            while (*source) *names++ = *source++;
            *names++ = '\0';
        }
        axes[index] = {name, axis.minimum, axis.def, axis.maximum, axis.tag, axis.strid};
    }
    for (std::size_t index = 0; index < numStyles; ++index) {
        const auto& style = native.namedstyle[index];
        styles[index] = {coordinates, style.strid, style.psid};
        for (std::size_t axis = 0; axis < numAxes; ++axis) coordinates[axis] = style.coords[axis];
        coordinates += numAxes;
    }
    return block;
}

struct MasterAxis64 {
    char* name;
    std::int64_t minimum;
    std::int64_t maximum;
};

struct MultiMaster64 {
    std::uint32_t numAxis;
    std::uint32_t numDesigns;
    MasterAxis64 axes[T1_MAX_MM_AXIS];
};

static_assert(sizeof(MasterAxis64) == 24 && offsetof(MultiMaster64, axes) == 8);

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
    FT_MM_Var* native = nullptr;
    const FT_Error error = FT_Get_MM_Var(owner ? owner->host : nullptr, master ? &native : nullptr);
    if (error != 0 || !master) return error;
    if constexpr (sizeof(FT_Long) == sizeof(std::int64_t)) {
        if (GuestCopyOut_nid_no_patch(master, &native, sizeof(native))) return 0;
        FT_Done_MM_Var(owner->library->host, native);
        return FT_Err_Invalid_Argument;
    } else {
        const auto release = [library = owner->library->host](FT_MM_Var* record) { FT_Done_MM_Var(library, record); };
        const std::unique_ptr<FT_MM_Var, decltype(release)> nativeRecord(native, release);
        auto block = PackMMVar(*native);
        if (!block) return FT_Err_Out_Of_Memory;
        void* result = block.get();
        if (!GuestCopyOut_nid_no_patch(master, &result, sizeof(result))) return FT_Err_Invalid_Argument;
        block.release();
        return 0;
    }
}

int APS5_VABI FT_Done_MM_Var_nid_postfix(GuestLibrary* library, void* master) {
    const auto host = ReadGuestLibrary(library, __func__);
    if constexpr (sizeof(FT_Long) == sizeof(std::int64_t)) {
        return FT_Done_MM_Var(host, static_cast<FT_MM_Var*>(master));
    } else {
        if (!host) return FT_Err_Invalid_Library_Handle;
        delete[] static_cast<std::byte*>(master);
        return 0;
    }
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
    FT_Multi_Master native{};
    const FT_Error error = FT_Get_Multi_Master(owner ? owner->host : nullptr, master ? &native : nullptr);
    if (error != 0 || !master) return error;
    if constexpr (sizeof(FT_Long) == sizeof(std::int64_t)) {
        const std::size_t size = offsetof(FT_Multi_Master, axis) + native.num_axis * sizeof(FT_MM_Axis);
        return GuestCopyOut_nid_no_patch(master, &native, size) ? 0 : FT_Err_Invalid_Argument;
    } else {
        MultiMaster64 guest;
        guest.numAxis = native.num_axis;
        guest.numDesigns = native.num_designs;
        for (FT_UInt index = 0; index < native.num_axis; ++index) {
            const auto& axis = native.axis[index];
            guest.axes[index] = {axis.name, axis.minimum, axis.maximum};
        }
        const std::size_t size = offsetof(MultiMaster64, axes) + guest.numAxis * sizeof(MasterAxis64);
        return GuestCopyOut_nid_no_patch(master, &guest, size) ? 0 : FT_Err_Invalid_Argument;
    }
}

}
