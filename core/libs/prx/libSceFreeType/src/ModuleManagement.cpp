#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MODULE_H
#include FT_RENDER_H

#include <cstdint>

using namespace FreeType;

extern "C" {

void* APS5_VABI FT_Get_Module_nid_postfix(GuestLibrary* library, const char* moduleName) {
    return FT_Get_Module(library ? library->host : nullptr, moduleName);
}

void* APS5_VABI FT_Get_Renderer_nid_postfix(GuestLibrary* library, std::uint32_t format) {
    return FT_Get_Renderer(library ? library->host : nullptr, static_cast<FT_Glyph_Format>(format));
}

int APS5_VABI FT_Remove_Module_nid_postfix(GuestLibrary* library, void* module) {
    return FT_Remove_Module(library ? library->host : nullptr, static_cast<FT_Module>(module));
}

void APS5_VABI FT_Add_Default_Modules_nid_postfix(GuestLibrary* library) {
    FT_Add_Default_Modules(library ? library->host : nullptr);
}

}
