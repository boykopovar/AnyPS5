#include "prx/libSceGvMp4Parser/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceGvMp4Parser/include/InventoryExports.hpp"
#include "prx/libSceGvMp4Parser/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <mutex>
#include <stdexcept>

namespace {

constexpr int kNotInitialized = static_cast<int>(0x809D420Bu);
constexpr std::size_t kParserBytes = 0x7b8;
constexpr std::size_t kHelperBytes = 0x28;
using Allocate = void* (APS5_VABI*)(std::size_t);
using Free = void (APS5_VABI*)(void*);

struct Config {
    Allocate allocate;
    std::uintptr_t other[2];
    Free release;
    std::uintptr_t rest[7];
};
static_assert(sizeof(Config) == 0x58);

struct Parser {
    Config config{};
    void* parser = nullptr;
    void* helper = nullptr;
    bool initializing = false;
    bool finalizePending = false;
};

struct GuestString {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(GuestString) == 32);

std::mutex g_mutex;
Parser g_parser;

void CopyIn(void* destination, const void* source, std::size_t size, const char* what) {
    const int saved = errno;
    const bool copied = source != nullptr && GuestCopyIn_nid_no_patch(destination, source, size);
    errno = saved;
    if (!copied) throw std::invalid_argument(what);
}

int Gate(const void* pathObject, const char* name) {
    GuestString path{};
    CopyIn(&path, pathObject, sizeof(path), "GvMp4 parser: unreadable path string");
    if (path.size > path.capacity) throw std::invalid_argument("GvMp4 parser: invalid path string size");
    bool initialized;
    {
        const std::lock_guard lock(g_mutex);
        initialized = g_parser.parser != nullptr || g_parser.helper != nullptr;
    }
    if (!initialized) {
        if (path.size > 15) throw std::logic_error("GvMp4 parser: the path copy calls the allocator before Initialize");
        return kNotInitialized;
    }
    NotImplemented_nid_no_patch(name);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceGvMp4ParserUnknown_sSswLa3vxMA_nid_no_patch(const void* config) {
    Config callbacks{};
    {
        const std::lock_guard lock(g_mutex);
        if (g_parser.parser || g_parser.helper || g_parser.initializing) return kNotInitialized;
        CopyIn(&callbacks, config, sizeof(callbacks), "GvMp4 parser: unreadable allocator table");
        if (!callbacks.allocate || !callbacks.release) throw std::invalid_argument("GvMp4 parser: missing allocator callback");
        g_parser.config = callbacks;
        g_parser.initializing = true;
    }
    void* parser = nullptr;
    void* helper = nullptr;
    try {
        parser = callbacks.allocate(kParserBytes);
        if (!parser) throw std::runtime_error("GvMp4 parser: a refused parser object allocation was not read on 13.42");
        helper = callbacks.allocate(kHelperBytes);
        if (!helper) throw std::runtime_error("GvMp4 parser: a refused second object allocation was not read on 13.42");
        {
            const std::lock_guard lock(g_mutex);
            if (!g_parser.finalizePending) {
                g_parser.initializing = false;
                g_parser.parser = parser;
                g_parser.helper = helper;
                return 0;
            }
        }
    } catch (...) {
        {
            const std::lock_guard lock(g_mutex);
            g_parser = {};
        }
        if (parser) callbacks.release(parser);
        throw;
    }
    {
        const std::lock_guard lock(g_mutex);
        g_parser = {};
    }
    callbacks.release(parser);
    callbacks.release(helper);
    return kNotInitialized;
}
APS5_EXPORT("sSswLa3vxMA", libSceGvMp4ParserUnknown_sSswLa3vxMA_nid_no_patch);

int APS5_VABI _ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() {
    Parser saved;
    {
        const std::lock_guard lock(g_mutex);
        if (g_parser.initializing) {
            g_parser.finalizePending = true;
            return 0;
        }
        saved = g_parser;
        g_parser = {};
    }
    if (saved.parser) saved.config.release(saved.parser);
    if (saved.helper) saved.config.release(saved.helper);
    return 0;
}

int APS5_VABI libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch(const void* path, void*, void*) {
    return Gate(path, __func__);
}
APS5_EXPORT("PJigiYrhUwQ", libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_tcsvdFwGn9U_nid_no_patch(const void* path, void*) {
    return Gate(path, __func__);
}
APS5_EXPORT("tcsvdFwGn9U", libSceGvMp4ParserUnknown_tcsvdFwGn9U_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_iPAZM6la8Gk_nid_no_patch(const void* path, std::uint32_t, std::uint32_t, void*) {
    return Gate(path, __func__);
}
APS5_EXPORT("iPAZM6la8Gk", libSceGvMp4ParserUnknown_iPAZM6la8Gk_nid_no_patch);

int APS5_VABI _ZN23sceMetadataReaderWriter11gvMp4Parser21gvMp4ParserInitializeEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter11gvMp4Parser23gvMp4ParserReadMetadataERKSsRKNS_8MetadataERS3_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter11gvMp4Parser24gvMp4ParserWriteMetadataERKSsRKNS_8MetadataE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter11gvMp4Parser27gvMp4ParserExtractThumbnailERKSsRNS_9ThumbnailE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceGvMp4ParserUnknown_1twpIQ9LVdk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1twpIQ9LVdk", libSceGvMp4ParserUnknown_1twpIQ9LVdk_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_2_minus_w0eKjKQ_plus_8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2-w0eKjKQ+8", libSceGvMp4ParserUnknown_2_minus_w0eKjKQ_plus_8_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_4BR2PtzlXac_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4BR2PtzlXac", libSceGvMp4ParserUnknown_4BR2PtzlXac_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_6sO25N6BpIE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6sO25N6BpIE", libSceGvMp4ParserUnknown_6sO25N6BpIE_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_Ar_plus_SWXuO_plus_B4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Ar+SWXuO+B4", libSceGvMp4ParserUnknown_Ar_plus_SWXuO_plus_B4_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_BL648cXsaOw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BL648cXsaOw", libSceGvMp4ParserUnknown_BL648cXsaOw_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_DxrIX_plus_BEzdA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DxrIX+BEzdA", libSceGvMp4ParserUnknown_DxrIX_plus_BEzdA_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_H1ACo_plus_TgX_plus_A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("H1ACo+TgX+A", libSceGvMp4ParserUnknown_H1ACo_plus_TgX_plus_A_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_Hi_plus_tZPfxucU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Hi+tZPfxucU", libSceGvMp4ParserUnknown_Hi_plus_tZPfxucU_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_Zqy0P4obozI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Zqy0P4obozI", libSceGvMp4ParserUnknown_Zqy0P4obozI_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_aVw8lQCnuYI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aVw8lQCnuYI", libSceGvMp4ParserUnknown_aVw8lQCnuYI_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_gYsaS58fv3Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gYsaS58fv3Y", libSceGvMp4ParserUnknown_gYsaS58fv3Y_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_hKxhdSPJr84_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hKxhdSPJr84", libSceGvMp4ParserUnknown_hKxhdSPJr84_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_mgcqnroon8s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mgcqnroon8s", libSceGvMp4ParserUnknown_mgcqnroon8s_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_uUeEONFSa30_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uUeEONFSa30", libSceGvMp4ParserUnknown_uUeEONFSa30_nid_no_patch);

int APS5_VABI libSceGvMp4ParserUnknown_ybFfESYz8VA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ybFfESYz8VA", libSceGvMp4ParserUnknown_ybFfESYz8VA_nid_no_patch);

}

extern "C" {

int APS5_VABI gvMp4LegacyUnknown__plus_iXAnntDUq8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+iXAnntDUq8", gvMp4LegacyUnknown__plus_iXAnntDUq8_nid_no_patch);

int APS5_VABI gvMp4LegacyUnknown_C23hWLV47ko_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("C23hWLV47ko", gvMp4LegacyUnknown_C23hWLV47ko_nid_no_patch);

int APS5_VABI gvMp4LegacyUnknown_qli8xXuToKQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qli8xXuToKQ", gvMp4LegacyUnknown_qli8xXuToKQ_nid_no_patch);

}
