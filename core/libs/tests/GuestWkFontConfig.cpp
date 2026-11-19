#include "prx/libSceWkFontConfig/libSceWkFontConfig.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

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


struct FontRecord {
    const char* path;
    const char* name;
    std::uint32_t attr[5];
    std::uint32_t tail;
};

template <typename Call>
bool Throws(Call call, const void* font, char* out, std::uint32_t size) {
    try {
        call(font, out, size);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void CheckRecordReads() {
    constexpr int bufferTooSmall = static_cast<int>(0x813C0002u);
    const FontRecord font{"/common/font/SST-Light.otf", "SST Light", {0x14, 1, 2, 0, 1}, 0xA5A5A5A5u};

    std::uint32_t attr[6] = {0x14, 9, 9, 9, 9, 0x77};
    Require(sceWkFontConfigGetAttr(&font, attr) == 0);
    Require(std::memcmp(attr, font.attr, sizeof(font.attr)) == 0 && attr[5] == 0x77);
    const FontRecord otherSize{nullptr, nullptr, {0x99, 6, 9, 2, 2}, 0};
    attr[0] = 0x14;
    Require(sceWkFontConfigGetAttr(&otherSize, attr) == 0);
    Require(attr[0] == 0x99 && attr[1] == 6 && attr[2] == 9 && attr[3] == 2 && attr[4] == 2);

    char out[32];
    std::memset(out, 'x', sizeof(out));
    Require(sceWkFontConfigGetFontName(&font, out, 16) == 0);
    Require(std::memcmp(out, "SST Light", 9) == 0);
    for (int index = 9; index < 16; ++index) Require(out[index] == '\0');
    for (int index = 16; index < 32; ++index) Require(out[index] == 'x');
    std::memset(out, 'x', sizeof(out));
    Require(sceWkFontConfigGetFontName(&font, out, 10) == 0 && std::strcmp(out, "SST Light") == 0 && out[10] == 'x');
    std::memset(out, 'x', sizeof(out));
    Require(sceWkFontConfigGetFontName(&font, out, 9) == bufferTooSmall && out[0] == 'x');
    Require(sceWkFontConfigGetFontName(&font, out, 0) == bufferTooSmall);
    const FontRecord empty{nullptr, "", {0x14, 0, 0, 0, 0}, 0};
    Require(sceWkFontConfigGetFontName(&empty, out, 1) == 0 && out[0] == '\0');
    Require(sceWkFontConfigGetFontName(&empty, out, 0) == bufferTooSmall);

    GuardedPage page;
    std::memcpy(page.data + page.size - 3, "SST", 3);
    const FontRecord runsIntoGuard{nullptr, reinterpret_cast<const char*>(page.data + page.size - 3), {0x14, 0, 0, 0, 0}, 0};
    Require(Throws(sceWkFontConfigGetFontName, &runsIntoGuard, out, sizeof(out)));
    const FontRecord nullName{nullptr, nullptr, {0x14, 0, 0, 0, 0}, 0};
    Require(Throws(sceWkFontConfigGetFontName, &nullName, out, sizeof(out)));
    std::uint32_t unread[5] = {0x14, 0, 0, 0, 0};
    bool threw = false;
    try {
        sceWkFontConfigGetAttr(page.data + page.size - 0x10 - 8, unread);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && unread[1] == 0);
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
    CheckRecordReads();
    return 0;
}
