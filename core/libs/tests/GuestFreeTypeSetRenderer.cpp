#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MODULE_H
#include FT_RENDER_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType set renderer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Libraries {
    GuestLibrary guest{};
    FT_Library native = nullptr;

    Libraries() { Require(FT_Init_FreeType(&guest.host) == 0 && FT_Init_FreeType(&native) == 0); }
    ~Libraries() { Require(FT_Done_FreeType(guest.host) == 0 && FT_Done_FreeType(native) == 0); }
};

class UnreadableParameters {
    void* memory;
    std::size_t size;

public:
    UnreadableParameters() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
        Require(memory != nullptr);
#else
        const auto pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        memory = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(memory != MAP_FAILED);
#endif
    }

    ~UnreadableParameters() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, size) == 0);
#endif
    }

    const GuestParameter* data() const { return static_cast<const GuestParameter*>(memory); }
};

int main() {
    for (const std::uint32_t format : {static_cast<std::uint32_t>(FT_GLYPH_FORMAT_OUTLINE), static_cast<std::uint32_t>(FT_GLYPH_FORMAT_BITMAP)}) {
        Libraries libraries;
        void* guestRenderer = FT_Get_Renderer(libraries.guest.host, static_cast<FT_Glyph_Format>(format));
        FT_Renderer nativeRenderer = FT_Get_Renderer(libraries.native, static_cast<FT_Glyph_Format>(format));
        Require((guestRenderer != nullptr) == (nativeRenderer != nullptr));
        Require(FT_Set_Renderer_nid_postfix(&libraries.guest, guestRenderer, 0, nullptr) == FT_Set_Renderer(libraries.native, nativeRenderer, 0, nullptr));
        Require(FT_Get_Renderer(libraries.guest.host, static_cast<FT_Glyph_Format>(format)) == guestRenderer);
        GuestParameter guestParameters[2] = {{0x12345678, nullptr}, {0x7fffffff, nullptr}};
        FT_Parameter nativeParameters[2] = {{0x12345678, nullptr}, {0x7fffffff, nullptr}};
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            Require(FT_Set_Renderer_nid_postfix(&libraries.guest, guestRenderer, count, guestParameters) ==
                    FT_Set_Renderer(libraries.native, nativeRenderer, count, nativeParameters));
        }
        Require(FT_Set_Renderer_nid_postfix(&libraries.guest, guestRenderer, 1, nullptr) == FT_Set_Renderer(libraries.native, nativeRenderer, 1, nullptr));
        Require(FT_Set_Renderer_nid_postfix(&libraries.guest, nullptr, 0, nullptr) == FT_Set_Renderer(libraries.native, nullptr, 0, nullptr));
        Require(FT_Set_Renderer_nid_postfix(nullptr, guestRenderer, 0, nullptr) == FT_Set_Renderer(nullptr, nativeRenderer, 0, nullptr));
    }
    Libraries first;
    Libraries second;
    void* foreign = FT_Get_Renderer(second.guest.host, FT_GLYPH_FORMAT_OUTLINE);
    FT_Renderer nativeForeign = FT_Get_Renderer(second.native, FT_GLYPH_FORMAT_OUTLINE);
    Require(FT_Set_Renderer_nid_postfix(&first.guest, foreign, 0, nullptr) == FT_Set_Renderer(first.native, nativeForeign, 0, nullptr));
    UnreadableParameters unreadable;
    void* local = FT_Get_Renderer(first.guest.host, FT_GLYPH_FORMAT_OUTLINE);
    Require(FT_Set_Renderer_nid_postfix(nullptr, local, 1, unreadable.data()) == FT_Set_Renderer(nullptr, static_cast<FT_Renderer>(local), 0, nullptr));
    Require(FT_Set_Renderer_nid_postfix(&first.guest, nullptr, 1, unreadable.data()) == FT_Set_Renderer(first.guest.host, nullptr, 0, nullptr));
    Require(FT_Set_Renderer_nid_postfix(&first.guest, foreign, 1, unreadable.data()) == FT_Set_Renderer(first.guest.host, static_cast<FT_Renderer>(foreign), 0, nullptr));
    GuestParameter wide[1] = {{0xFFFFFFFFFFFFFFFFull, nullptr}};
    FT_Parameter nativeWide[1] = {{static_cast<FT_ULong>(0xFFFFFFFFFFFFFFFFull), nullptr}};
    if (sizeof(FT_ULong) == 8) {
        Require(FT_Set_Renderer_nid_postfix(&first.guest, FT_Get_Renderer(first.guest.host, FT_GLYPH_FORMAT_OUTLINE), 1, wide) ==
                FT_Set_Renderer(first.native, FT_Get_Renderer(first.native, FT_GLYPH_FORMAT_OUTLINE), 1, nativeWide));
    }
}
