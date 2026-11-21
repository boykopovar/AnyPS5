#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MODULE_H
#include FT_RENDER_H

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace FreeType;

namespace {

bool CopyInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

bool ReadLibrary(const GuestLibrary* library, FT_Library& hostLibrary) {
    hostLibrary = nullptr;
    if (!library) return true;
    return CopyInPreservingErrno(&hostLibrary, &library->host, sizeof(hostLibrary));
}

bool ReadParameter(const GuestParameter* parameters, std::uint32_t index, GuestParameter& out) {
    const auto base = reinterpret_cast<std::uintptr_t>(parameters);
    const auto offset = static_cast<std::uintptr_t>(index) * sizeof(GuestParameter);
    if (offset / sizeof(GuestParameter) != index || base > std::numeric_limits<std::uintptr_t>::max() - offset) return false;
    return CopyInPreservingErrno(&out, reinterpret_cast<const void*>(base + offset), sizeof(out));
}

}

extern "C" {

int APS5_VABI FreeTypeSetRenderer_nid_no_patch(GuestLibrary* library, void* renderer, std::uint32_t count, const GuestParameter* parameters) {
    FT_Library hostLibrary = nullptr;
    if (!ReadLibrary(library, hostLibrary)) return FT_Err_Invalid_Argument;
    auto hostRenderer = static_cast<FT_Renderer>(renderer);
    const int validation = FT_Set_Renderer(hostLibrary, hostRenderer, 0, nullptr);
    if (validation != 0) return validation;
    std::vector<FT_Parameter> native;
    if (parameters && count != 0) {
        native.reserve(count < 64u ? count : 64u);
        for (std::uint32_t index = 0; index < count; ++index) {
            GuestParameter parameter{};
            if (!ReadParameter(parameters, index, parameter)) return FT_Err_Invalid_Argument;
            if (parameter.tag > std::numeric_limits<FT_ULong>::max()) {
                throw std::runtime_error("FT_Set_Renderer: a parameter tag outside the host FT_ULong range is not implemented");
            }
            native.push_back({static_cast<FT_ULong>(parameter.tag), parameter.data});
        }
    }
    return FT_Set_Renderer(hostLibrary, hostRenderer, count, parameters ? native.data() : nullptr);
}

int APS5_VABI FT_Set_Renderer_nid_postfix(GuestLibrary* library, void* renderer, std::uint32_t count, const GuestParameter* parameters) {
    return FreeTypeSetRenderer_nid_no_patch(library, renderer, count, parameters);
}

}
