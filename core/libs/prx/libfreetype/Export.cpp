#include "prx/libfreetype/include/LegacyExports.hpp"
#include "prx/libfreetype/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include FT_MODULE_H
#ifdef ANYPS5_BUNDLED_FREETYPE
#include <freetype/internal/ftobjs.h>
#endif

#include <string>

extern "C" {

#ifdef ANYPS5_BUNDLED_FREETYPE
void APS5_VABI FT_Set_Default_Properties_nid_postfix(FreeType::GuestLibrary* library) {
    std::string environment;
    if (!CopyGuestEnvironment_nid_no_patch("FREETYPE_PROPERTIES", environment)) return;
    FT_Library host = library ? library->host : nullptr;
    std::size_t position = 0;
    while (position < environment.size()) {
        if (environment[position] == ' ' || environment[position] == '\t') {
            ++position;
            continue;
        }
        const auto separator = environment.find(':', position);
        if (separator == std::string::npos || separator == position || separator - position > 128) break;
        const auto assignment = environment.find('=', separator + 1);
        if (assignment == std::string::npos || assignment == separator + 1 || assignment - separator - 1 > 128) break;
        const auto end = environment.find_first_of(" \t", assignment + 1);
        const auto valueEnd = end == std::string::npos ? environment.size() : end;
        if (valueEnd == assignment + 1 || valueEnd - assignment - 1 > 128) break;
        const std::string module = environment.substr(position, separator - position);
        const std::string property = environment.substr(separator + 1, assignment - separator - 1);
        std::string value = environment.substr(assignment + 1, valueEnd - assignment - 1);
        ft_property_string_set(host, module.c_str(), property.c_str(), value.data());
        position = valueEnd;
    }
}
#else
void APS5_VABI FT_Set_Default_Properties_nid_postfix(FreeType::GuestLibrary* library) {
    std::string environment;
    if (library && CopyGuestEnvironment_nid_no_patch("FREETYPE_PROPERTIES", environment)) NotImplemented_nid_no_patch(__func__);
}
#endif

}

extern "C" {

int APS5_VABI freetypeLegacyFtcManagerNew_nid_no_patch(FreeType::GuestLibrary* library, std::uint32_t maxFaces, std::uint32_t maxSizes,
                                                      std::uint64_t maxBytes, FreeType::GuestFaceRequester requester, void* reqData, void** manager) {
    return FreeType::FreeTypeNewCacheManager_nid_no_patch(library, maxFaces, maxSizes, maxBytes, requester, reqData, manager, __func__);
}
APS5_EXPORT("DYbLD-s1oiQ", freetypeLegacyFtcManagerNew_nid_no_patch);


const char* APS5_VABI FT_Error_String_nid_postfix(std::int32_t errorCode) {
    return FT_Error_String(errorCode);
}


int APS5_VABI freetypeLegacyNewGlyph_nid_no_patch(FreeType::GuestLibrary* library, std::uint32_t format, FreeType::GuestGlyphRec** target) {
    return FreeType::FreeTypeNewGlyph_nid_no_patch(library, format, target);
}
APS5_EXPORT("sZq9oSwMGSs", freetypeLegacyNewGlyph_nid_no_patch);

}
