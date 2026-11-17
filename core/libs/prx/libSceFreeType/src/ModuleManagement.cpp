#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MODULE_H
#include FT_RENDER_H

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

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
    if (!CopyInPreservingErrno(&host, &library->host, sizeof(host))) NotImplemented_nid_no_patch(func);
    return host;
}

std::string CopyGuestCString(const char* source, const char* func) {
    std::string result;
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    std::size_t offset = 0;
    for (;;) {
        if (offset > std::numeric_limits<std::uintptr_t>::max() - base) NotImplemented_nid_no_patch(func);
        const auto current = base + offset;
        const std::size_t toPageEnd = 4096 - (current % 4096);
        const std::size_t chunk = toPageEnd < 256 ? toPageEnd : 256;
        char buffer[256];
        if (!CopyInPreservingErrno(buffer, reinterpret_cast<const void*>(current), chunk)) {
            NotImplemented_nid_no_patch(func);
        }
        if (const auto* end = static_cast<const char*>(std::memchr(buffer, '\0', chunk))) {
            result.append(buffer, static_cast<std::size_t>(end - buffer));
            return result;
        }
        result.append(buffer, chunk);
        offset += chunk;
    }
}

}

extern "C" {

void* APS5_VABI FT_Get_Module_nid_postfix(GuestLibrary* library, const char* moduleName) {
    const FT_Library host = ReadGuestLibrary(library, __func__);
    if (!host || !moduleName) return FT_Get_Module(host, nullptr);
    const std::string name = CopyGuestCString(moduleName, __func__);
    return FT_Get_Module(host, name.c_str());
}

void* APS5_VABI FT_Get_Renderer_nid_postfix(GuestLibrary* library, std::uint32_t format) {
    const FT_Library host = ReadGuestLibrary(library, __func__);
    if (!InGlyphFormatRange(format)) return nullptr;
    return FT_Get_Renderer(host, static_cast<FT_Glyph_Format>(format));
}

int APS5_VABI FT_Remove_Module_nid_postfix(GuestLibrary* library, void* module) {
    return FT_Remove_Module(ReadGuestLibrary(library, __func__), static_cast<FT_Module>(module));
}

void APS5_VABI FT_Add_Default_Modules_nid_postfix(GuestLibrary* library) {
    FT_Add_Default_Modules(ReadGuestLibrary(library, __func__));
}

}
