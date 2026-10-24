#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include FT_MODULE_H

#include <cstdint>

extern "C" {

void APS5_VABI FT_Library_Version_nid_postfix(FreeType::GuestLibrary* library, std::int32_t* major, std::int32_t* minor, std::int32_t* patch) {
    FT_Int hostMajor = 0;
    FT_Int hostMinor = 0;
    FT_Int hostPatch = 0;
    FT_Library_Version(library ? library->host : nullptr, &hostMajor, &hostMinor, &hostPatch);
    if (major) *major = hostMajor;
    if (minor) *minor = hostMinor;
    if (patch) *patch = hostPatch;
}

std::int32_t APS5_VABI FT_Get_TrueType_Engine_Type_nid_postfix(FreeType::GuestLibrary* library) {
    return static_cast<std::int32_t>(FT_Get_TrueType_Engine_Type(library ? library->host : nullptr));
}

}
