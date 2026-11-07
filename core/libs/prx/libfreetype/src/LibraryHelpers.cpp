#include "prx/libfreetype/include/LibraryHelpers.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include FT_LCD_FILTER_H
#include FT_MULTIPLE_MASTERS_H

#include <array>
#include <cstddef>
#include <utility>

static_assert(sizeof(FT_MM_Var) == 32 && offsetof(FT_MM_Var, num_namedstyles) == 8 && offsetof(FT_MM_Var, axis) == 16);
static_assert(sizeof(FT_UInt) == sizeof(std::uint32_t) && sizeof(FreeType::GuestVector) == 16);

extern "C" {

int APS5_VABI freetypeLegacyLibrarySetLcdGeometry_nid_no_patch(FreeType::GuestLibrary* library, const FreeType::GuestVector* sub) {
    const FT_Library host = library ? library->host : nullptr;
    if (!host || !sub) return FT_Library_SetLcdGeometry(host, nullptr);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(sub, sizeof(*sub) * 3);
    std::array<FT_Vector, 3> native{};
    for (std::size_t index = 0; index < native.size(); ++index) {
        if (!std::in_range<FT_Pos>(sub[index].x) || !std::in_range<FT_Pos>(sub[index].y)) NotImplemented_nid_no_patch(__func__);
        native[index] = {static_cast<FT_Pos>(sub[index].x), static_cast<FT_Pos>(sub[index].y)};
    }
    return FT_Library_SetLcdGeometry(host, native.data());
}
APS5_EXPORT("Xxtq+Qr5lzI", freetypeLegacyLibrarySetLcdGeometry_nid_no_patch);

int APS5_VABI libfreetypeGetVarAxisFlags_nid_no_patch(void* master, std::uint32_t axisIndex, std::uint32_t* flags) {
    if (!master || !flags) return FT_Get_Var_Axis_Flags(nullptr, axisIndex, nullptr);
    auto* native = static_cast<FT_MM_Var*>(master);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(native, sizeof(*native));
    if (axisIndex < native->num_axis) {
        GuestArena::GuestArenaPrepareHostRead_nid_no_patch(reinterpret_cast<const FT_UShort*>(native + 1) + axisIndex, sizeof(FT_UShort));
    }
    FT_UInt value = 0;
    const FT_Error error = FT_Get_Var_Axis_Flags(native, axisIndex, &value);
    if (error) return error;
    GuestArena::HostWrite write(flags, sizeof(*flags));
    if (!write.Open()) return FT_Err_Invalid_Argument;
    *flags = value;
    return 0;
}
APS5_EXPORT("+sDVkl3aAR8", libfreetypeGetVarAxisFlags_nid_no_patch);

}
