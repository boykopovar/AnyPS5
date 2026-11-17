#include "prx/libSceFreeTypeOt/include/Library.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI FT_Reference_Library_nid_postfix(FreeType::GuestLibrary* library) {
    return FreeTypeReferenceLibrary_nid_no_patch(library);
}

int APS5_VABI FT_New_Library_nid_postfix(FreeType::GuestMemoryRec* memory, FreeType::GuestLibrary** alibrary) {
    return FreeTypeNewLibrary_nid_no_patch(memory, alibrary);
}

}

#if defined(ANYPS5_FREETYPEOT_SYSTEM_DEPS)
extern "C" void* APS5_VABI FT_Lookup_Renderer_nid_postfix(FreeType::GuestLibrary*, std::uint32_t, void**) {
    NotImplemented_nid_no_patch(__func__);
}
#else
#include <freetype/internal/ftobjs.h>

#include <cerrno>
#include <cstddef>

namespace {

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

FT_Library ReadLibrary(FreeType::GuestLibrary* library) {
    FT_Library host = nullptr;
    if (!CopyIn(&host, &library->host, sizeof(host)) || !host) NotImplemented_nid_no_patch("FT_Lookup_Renderer");
    return host;
}

FT_ListNode ReadNode(FT_Library library, void** node) {
    FT_ListNode current = nullptr;
    if (!CopyIn(&current, node, sizeof(current))) NotImplemented_nid_no_patch("FT_Lookup_Renderer");
    if (!current) return nullptr;
    FT_ListNode entry = library->renderers.head;
    while (entry && entry != current) entry = entry->next;
    if (!entry) NotImplemented_nid_no_patch("FT_Lookup_Renderer");
    return current;
}

void WriteNode(void** node, FT_ListNode value) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(node, &value, sizeof(value));
    errno = saved;
    if (!ok) NotImplemented_nid_no_patch("FT_Lookup_Renderer");
}

}

extern "C" void* APS5_VABI FT_Lookup_Renderer_nid_postfix(FreeType::GuestLibrary* library, std::uint32_t format, void** node) {
    if (!library) return nullptr;
    const FT_Library host = ReadLibrary(library);
    const auto glyphFormat = static_cast<FT_Glyph_Format>(format);
    if (!node) return FT_Lookup_Renderer(host, glyphFormat, nullptr);
    FT_ListNode current = ReadNode(host, node);
    const FT_Renderer renderer = FT_Lookup_Renderer(host, glyphFormat, &current);
    WriteNode(node, current);
    return renderer;
}
#endif
