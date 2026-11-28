#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MODULE_H
#include FT_RENDER_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace FreeType;

extern "C" {

int APS5_VABI FreeTypeSetRenderer_nid_no_patch(GuestLibrary* library, void* renderer, std::uint32_t count, const GuestParameter* parameters) {
    auto hostLibrary = library ? library->host : nullptr;
    auto hostRenderer = static_cast<FT_Renderer>(renderer);
    const int validation = FT_Set_Renderer(hostLibrary, hostRenderer, 0, nullptr);
    if (validation != 0) return validation;
    std::vector<FT_Parameter> native;
    if (parameters) {
        native.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            if (parameters[index].tag > std::numeric_limits<FT_ULong>::max()) {
                throw std::runtime_error("FT_Set_Renderer: a parameter tag outside the host FT_ULong range is not implemented");
            }
            native.push_back({static_cast<FT_ULong>(parameters[index].tag), parameters[index].data});
        }
    }
    return FT_Set_Renderer(hostLibrary, hostRenderer, count, parameters ? native.data() : nullptr);
}

int APS5_VABI FT_Set_Renderer_nid_postfix(GuestLibrary* library, void* renderer, std::uint32_t count, const GuestParameter* parameters) {
    return FreeTypeSetRenderer_nid_no_patch(library, renderer, count, parameters);
}

}
