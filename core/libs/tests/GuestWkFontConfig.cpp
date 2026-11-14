#include "prx/libSceWkFontConfig/libSceWkFontConfig.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

struct FontAttr {
    std::uint32_t size;
    std::uint32_t field4;
    std::uint32_t field8;
    std::uint32_t fieldC;
    std::uint32_t field10;
};

class GuardedPage {
public:
    unsigned char* data = nullptr;
    std::size_t size = 0;
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const auto pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        auto* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED);
        data = static_cast<unsigned char*>(mapping);
        Require(mprotect(data + size, size, PROT_NONE) == 0);
#endif
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;
};

void RequireEarlyReturnBeforeUnreadableTail(int invalidArgument) {
    const void* fontOut = nullptr;
    GuardedPage page;
    constexpr std::array<std::uint32_t, 5> valid{0x14, 0, 0, 0, 0};
    constexpr std::array<std::uint32_t, 5> invalid{0x10, 7, 10, 3, 3};
    for (std::size_t failing = 0; failing < valid.size(); ++failing) {
        unsigned char* block = page.data + page.size - (failing + 1) * sizeof(std::uint32_t);
        for (std::size_t field = 0; field <= failing; ++field) {
            const std::uint32_t value = field == failing ? invalid[field] : valid[field];
            std::memcpy(block + field * sizeof(std::uint32_t), &value, sizeof(value));
        }
        Require(sceWkFontConfigGetFont(&fontOut, 0, block) == invalidArgument);
    }
}

} // namespace

int main() {
    constexpr int invalidArgument = static_cast<int>(0x813C0001u);
    const void* fontOut = nullptr;
    char text[64] = {};
    const char name[] = "font";
    int fontTarget = 0;
    for (int call = 0; call < 2; ++call) {
        Require(sceWkFontConfigGetFont(nullptr, 0, nullptr) == invalidArgument);
        Require(sceWkFontConfigGetFont(&fontOut, 0x96, nullptr) == invalidArgument);
        Require(sceWkFontConfigGetFont(&fontOut, 0xFFFFFFFFu, nullptr) == invalidArgument);
        for (const FontAttr& attr : std::array<FontAttr, 6>{FontAttr{0x10, 0, 0, 0, 0}, FontAttr{0x14, 7, 0, 0, 0},
                                                             FontAttr{0x14, 0, 10, 0, 0}, FontAttr{0x14, 0, 0, 3, 0},
                                                             FontAttr{0x14, 0, 0, 0, 3},
                                                             FontAttr{0x14, 0xFFFFFFFFu, 0, 0, 0}}) {
            Require(sceWkFontConfigGetFont(&fontOut, 0, &attr) == invalidArgument);
        }
        Require(sceWkFontConfigGetFontFromFontName(nullptr, name) == invalidArgument);
        Require(sceWkFontConfigGetFontFromFontName(&fontOut, nullptr) == invalidArgument);
        Require(sceWkFontConfigGetFontName(nullptr, text, sizeof(text)) == invalidArgument);
        Require(sceWkFontConfigGetFontName(&fontTarget, nullptr, sizeof(text)) == invalidArgument);
        Require(sceWkFontConfigGetFontPath(nullptr, text, sizeof(text)) == invalidArgument);
        Require(sceWkFontConfigGetFontPath(&fontTarget, nullptr, sizeof(text)) == invalidArgument);
        Require(sceWkFontConfigGetAttr(&fontTarget, nullptr) == invalidArgument);
        for (std::uint32_t size : std::array<std::uint32_t, 3>{0u, 0x13u, 0x15u}) {
            FontAttr attr{size, 0, 0, 0, 0};
            Require(sceWkFontConfigGetAttr(&fontTarget, &attr) == invalidArgument);
        }
    }
    RequireEarlyReturnBeforeUnreadableTail(invalidArgument);
    return 0;
}
