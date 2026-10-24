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

}
