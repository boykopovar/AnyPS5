#include "prx/libSceWkFontConfig/libSceWkFontConfig.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

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
    explicit GuardedPage(bool guardFirst = false) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(data + (guardFirst ? 0 : size), size, PAGE_NOACCESS, &previous) != 0);
#else
        const auto pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        auto* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED);
        data = static_cast<unsigned char*>(mapping);
        Require(mprotect(data + (guardFirst ? 0 : size), size, PROT_NONE) == 0);
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

void CheckAttributeCopyOrder() {
    std::array<std::uint32_t, 10> words{0, 0, 0, 0x14, 0x14, 1, 2, 3, 4, 0x77};
    Require(sceWkFontConfigGetAttr(words.data(), words.data() + 3) == 0);
    Require(words == std::array<std::uint32_t, 10>{0, 0, 0, 0x14, 1, 2, 4, 4, 4, 0x77});

    GuardedPage page(true);
    const std::uint32_t tail = 0x12345678;
    std::memcpy(page.data + page.size, &tail, sizeof(tail));
    std::array<std::uint32_t, 5> attr{0x14, 1, 2, 3, 4};
    bool threw = false;
    try {
        sceWkFontConfigGetAttr(page.data + page.size - 0x20, attr.data());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && attr == std::array<std::uint32_t, 5>{0x14, 1, 2, 3, tail});
}

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
    std::array<char, 10003> padded;
    padded.fill('x');
    Require(sceWkFontConfigGetFontName(&font, padded.data(), padded.size() - 1) == 0);
    Require(std::strcmp(padded.data(), font.name) == 0);
    for (std::size_t i = std::strlen(font.name); i < padded.size() - 1; ++i) Require(padded[i] == '\0');
    Require(padded.back() == 'x');
    Require(Throws(sceWkFontConfigGetFontName, &font, reinterpret_cast<char*>(page.data + page.size), UINT32_MAX));

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
const FontRecord* Select(std::uint32_t fontId, std::uint32_t family, std::uint32_t weight, std::uint32_t style) {
    const FontAttr attr{0x14, family, weight, style, 0};
    const void* font = nullptr;
    Require(sceWkFontConfigGetFont(&font, fontId, &attr) == 0 && font != nullptr);
    return static_cast<const FontRecord*>(font);
}

std::string Name(const FontRecord* font) { return font->name; }

void CheckFontTable() {
    constexpr int bufferTooSmall = static_cast<int>(0x813C0002u);
    constexpr int notFound = static_cast<int>(0x813C0003u);
    const void* font = nullptr;
    Require(sceWkFontConfigGetFont(&font, 25, nullptr) == 0 && Name(static_cast<const FontRecord*>(font)) == "SST Medium");
    Require(Name(Select(25, 0, 0, 0)) == "SST Medium");
    Require(Name(Select(25, 0, 1, 0)) == "SST Light");
    Require(Name(Select(25, 0, 3, 0)) == "SST Light");
    Require(Name(Select(25, 0, 6, 0)) == "SST Medium");
    Require(Name(Select(8, 0, 7, 0)) == "SST Bold");
    Require(Name(Select(14, 0, 9, 0)) == "SST Bold");
    Require(Name(Select(25, 0, 4, 1)) == "SST Medium Italic");
    Require(Name(Select(25, 0, 3, 1)) == "SST Light Italic");
    Require(Name(Select(25, 3, 0, 1)) == "SST Typewriter Roman");
    Require(Name(Select(25, 3, 5, 0)) == "SST Typewriter Roman");
    Require(Name(Select(25, 3, 3, 0)) == "SST Typewriter Roman");
    Require(Name(Select(25, 3, 8, 0)) == "SST Typewriter Bold");
    Require(Name(Select(0, 3, 0, 1)) == "SST Japanese Pro Regular");
    Require(Name(Select(20, 0, 7, 0)) == "SST Japanese Pro Bold");
    Require(Name(Select(108, 0, 5, 0)) == "SST Japanese Pro Regular");
    Require(Name(Select(2, 0, 0, 0)) == "SST Arabic Medium");
    Require(Name(Select(1, 0, 3, 0)) == "SST Arabic Light");
    Require(Name(Select(18, 0, 0, 0)) == "SCEPS4Yoongd-Medium");
    Require(Name(Select(122, 0, 7, 0)) == "SCEPS4Yoongd-Bold");
    for (std::uint32_t fontId : {3u, 19u, 26u, 109u, 121u, 123u, 0x95u}) Require(Name(Select(fontId, 3, 7, 1)) == "DFHeiW5-A-SONY Regular");
    int language = -1;
    const bool chineseOrKorean = sceSystemServiceParamGetInt(1, &language) == 0 && language >= 9 && language <= 11;
    Require(Name(Select(17, 0, 7, 0)) == (chineseOrKorean ? "DFHeiW5-A-SONY Regular" : "SST Japanese Pro Bold"));

    const FontRecord* typewriterBold = Select(25, 3, 7, 0);
    std::uint32_t attr[5] = {0x14, 0, 0, 0, 0};
    Require(sceWkFontConfigGetAttr(typewriterBold, attr) == 0);
    Require(attr[0] == 0x14 && attr[1] == 3 && attr[2] == 7 && attr[3] == 2 && attr[4] == 2);

    const void* found = nullptr;
    Require(sceWkFontConfigGetFontFromFontName(&found, "SST Bold Italic") == 0 && found == Select(25, 0, 7, 1));
    Require(sceWkFontConfigGetFontFromFontName(&found, "DFHeiW5-A-SONY Regular") == 0 && found == Select(3, 0, 0, 0));
    Require(sceWkFontConfigGetFontFromFontName(&found, "SCEPS4Yoongd-Light") == 0 && found == Select(18, 0, 3, 0));
    found = &attr;
    Require(sceWkFontConfigGetFontFromFontName(&found, "SST") == notFound && found == &attr);
    Require(sceWkFontConfigGetFontFromFontName(&found, "sst bold") == notFound && found == &attr);
    Require(sceWkFontConfigGetFontFromFontName(&found, "") == notFound && found == &attr);

    GuardedPage page;
    page.data[page.size - 1] = 'X';
    Require(sceWkFontConfigGetFontFromFontName(&found, reinterpret_cast<const char*>(page.data + page.size - 1)) == notFound);
    std::memcpy(page.data + page.size - 3, "SST", 3);
    bool threw = false;
    try {
        sceWkFontConfigGetFontFromFontName(&found, reinterpret_cast<const char*>(page.data + page.size - 3));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && found == &attr);

    const FontRecord* medium = Select(25, 0, 0, 0);
    const std::string expected = std::string("/") + sceKernelGetFsSandboxRandomWord() + "/common/font/SST-Medium.otf";
    char path[128];
    std::memset(path, 'x', sizeof(path));
    Require(sceWkFontConfigGetFontPath(medium, path, static_cast<std::uint32_t>(expected.size())) == bufferTooSmall && path[0] == 'x');
    Require(sceWkFontConfigGetFontPath(medium, path, static_cast<std::uint32_t>(expected.size() + 1)) == 0);
    Require(expected == path && path[expected.size() + 1] == 'x');
    const std::string longPath(0x3ff, 'p');
    const FontRecord longest{longPath.c_str(), "long", {0x14, 0, 0, 0, 0}, 0};
    std::string large(0x800, 'x');
    Require(sceWkFontConfigGetFontPath(&longest, large.data(), static_cast<std::uint32_t>(large.size())) == 0);
    Require(std::string(large.c_str()) == std::string("/") + sceKernelGetFsSandboxRandomWord() + longPath);
    const std::string tooLong(0x400, 'p');
    const FontRecord overLimit{tooLong.c_str(), "long", {0x14, 0, 0, 0, 0}, 0};
    Require(sceWkFontConfigGetFontPath(&overLimit, large.data(), static_cast<std::uint32_t>(large.size())) == bufferTooSmall);
    const FontRecord nullPath{nullptr, "none", {0x14, 0, 0, 0, 0}, 0};
    Require(Throws(sceWkFontConfigGetFontPath, &nullPath, path, sizeof(path)));
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
    CheckAttributeCopyOrder();
    CheckRecordReads();
    CheckFontTable();
    return 0;
}
