#include "prx/libSceFont/include/FontInternal.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" {
int APS5_VABI sceFontAttachDeviceCacheBuffer(FontLibrary library, void* buffer, std::uint32_t size);
int APS5_VABI sceFontDettachDeviceCacheBuffer(FontLibrary library, void** buffer, std::uint32_t* size);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font device cache buffer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    Font::FontLibNative lib{};
    lib.magic = Font::LIBRARY_MAGIC;
    const FontLibrary library = reinterpret_cast<FontLibrary>(&lib);
    std::vector<std::uint32_t> storage(0x3000 / 4);
    void* const buffer = storage.data();

    void* out = reinterpret_cast<void*>(1);
    std::uint32_t size = 1;

    Require(sceFontDettachDeviceCacheBuffer(library, &out, &size) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER &&
            out == nullptr && size == 0);

    Require(sceFontAttachDeviceCacheBuffer(library, buffer, 0x3000) == SCE_FONT_OK);
    out = reinterpret_cast<void*>(1);
    size = 1;
    Require(sceFontDettachDeviceCacheBuffer(library, &out, &size) == SCE_FONT_OK && out == buffer && size == 0x3000);

    out = reinterpret_cast<void*>(1);
    size = 1;
    Require(sceFontDettachDeviceCacheBuffer(library, &out, &size) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER &&
            out == nullptr && size == 0);

    Require(sceFontAttachDeviceCacheBuffer(library, buffer, 0x3000) == SCE_FONT_OK);
    out = reinterpret_cast<void*>(1);
    size = 1;
    Require(sceFontDettachDeviceCacheBuffer(library, &out, &size) == SCE_FONT_OK && out == buffer && size == 0x3000);

    Require(sceFontDettachDeviceCacheBuffer(library, nullptr, &size) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontDettachDeviceCacheBuffer(library, &out, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontDettachDeviceCacheBuffer(nullptr, &out, &size) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(out == nullptr && size == 0);

    return 0;
}