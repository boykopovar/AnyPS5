#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int ERR_INVALID_PARAMETER = static_cast<int>(0x805C0001);
constexpr int ERR_INVALID_SRC_BUFFER = static_cast<int>(0x805C0010);
constexpr int ERR_INVALID_ENCODE = static_cast<int>(0x805C0014);
constexpr int ERR_ILLEGAL_CODE = static_cast<int>(0x805C0015);
constexpr int ERR_INVALID_DST_BUFFER = static_cast<int>(0x805C0030);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CES guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using SheetInit = void* (APS5_VABI*)(void*);

void* const UNMAPPED = reinterpret_cast<void*>(0x10);

}

int main() {
    constexpr int savedErrno = 71;
    const uint8_t* cp1252 = sceCesRefersUcsProfileCp1252();
    const uint8_t ascii[] = {'A'};
    uint8_t zeroed[0x100] = {};
    int marker = 0;
    const void* profile = &marker;

    const SheetInit sheets[] = {
        sceCesUcsProfileInitSJis1997Cp932,
        sceCesUcsProfileInitSJis1997X0208,
        sceCesUcsProfileInitEucJpCp51932,
        sceCesUcsProfileInitEucJpX0208,
        sceCesUcsProfileInitEucJpX0208Ss2,
        sceCesUcsProfileInitEucJpX0208Ss2Ss3,
        sceCesUcsProfileInitEucKr,
        sceCesUcsProfileInitUhc,
        sceCesUcsProfileInitBig5Cp950,
        sceCesUcsProfileInitEucCnGb2312,
        sceCesUcsProfileInitEucJis2004,
        sceCesUcsProfileInitSJis2004X0213,
        sceCesUcsProfileInitIso2022JpRfc1468,
        sceCesUcsProfileInitIso2022JpCp50221,
        sceCesUcsProfileInitIso2022JpCp50222,
    };
    for (const SheetInit init : sheets) {
        errno = savedErrno;
        Require(init(UNMAPPED) == nullptr);
        Require(errno == savedErrno);
    }

    errno = savedErrno;
    Require(sceCesMbcsUcsContextInit(UNMAPPED, profile) == ERR_INVALID_PARAMETER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesMbcsUcsContextInitCopy(UNMAPPED, zeroed) == ERR_INVALID_PARAMETER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesIso2022UcsContextInit(UNMAPPED, nullptr) == ERR_INVALID_PARAMETER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesIso2022UcsContextInit(UNMAPPED, profile) == ERR_INVALID_PARAMETER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesIso2022UcsContextInitCopy(UNMAPPED, zeroed) == ERR_INVALID_PARAMETER);
    Require(errno == savedErrno);

    auto* const unmappedU32 = reinterpret_cast<uint32_t*>(0x10);
    auto* const unmappedU16 = reinterpret_cast<uint16_t*>(0x10);
    auto* const unmappedU8 = reinterpret_cast<uint8_t*>(0x10);
    uint32_t size = 0;
    uint16_t unit = 0;
    uint8_t byte = 0;

    errno = savedErrno;
    Require(sceCesEucJpToUcs2(nullptr, nullptr, 0, unmappedU32, nullptr) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesEucJpToUcs2(nullptr, nullptr, 0, nullptr, unmappedU16) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);

    errno = savedErrno;
    Require(sceCesSbcToUtf8(cp1252, 'A', unmappedU8, 4, nullptr) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesSbcToUtf8(cp1252, 'A', nullptr, 4, unmappedU32) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);

    errno = savedErrno;
    Require(sceCesUtf8ToSbc(ascii, 1, unmappedU32, nullptr, nullptr) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesUtf8ToSbc(ascii, 1, nullptr, cp1252, unmappedU8) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);

    errno = savedErrno;
    Require(sceCesEucJpToUtf8(nullptr, ascii, 1, unmappedU32, nullptr, 0, nullptr) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);

    errno = savedErrno;
    Require(sceCesSJisCodeToJisX0208(0x8140, unmappedU8, &byte) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesJisX0208ToSJisCode(0x30, 0x21, unmappedU16) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesSJisCodeToJisX0213(0x8140, unmappedU8, &byte, &byte) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesJisGetLevel(1, 16, 1, unmappedU32) == ERR_INVALID_PARAMETER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesJisGetLevel(0, 1, 1, unmappedU32) == ERR_INVALID_PARAMETER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesJisX0213ToSJisCode(1, 16, 1, unmappedU16) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);

    errno = savedErrno;
    Require(sceCesSJisGetCode(reinterpret_cast<const uint8_t*>(0x10), 2, &size, &unit) == ERR_INVALID_SRC_BUFFER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesSJisGetCode(ascii, 1, unmappedU32, &unit) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);

    errno = savedErrno;
    Require(sceCesSJisPutCode(0x8140, unmappedU8, 2, &size) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesSJisPutCode(0x41, &byte, 1, unmappedU32) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);

    errno = savedErrno;
    Require(sceCesSbcToUcs2(cp1252, 'A', unmappedU16) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);

    errno = savedErrno;
    Require(sceCesUtf8ToUcs2(ascii, 1, nullptr, unmappedU16) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);
    errno = savedErrno;
    Require(sceCesUtf8ToUcs2(ascii, 1, unmappedU32, &unit) == ERR_INVALID_DST_BUFFER);
    Require(errno == savedErrno);

    alignas(8) uint8_t sjisSheet[64] = {};
    alignas(8) uint8_t eucKrSheet[64] = {};
    const void* sjis = sceCesUcsProfileInitSJis1997Cp932(sjisSheet);
    const void* eucKr = sceCesUcsProfileInitEucKr(eucKrSheet);
    Require(sjis != nullptr && eucKr != nullptr);
    uint8_t utf8[4] = {};
    uint16_t utf16[2] = {};
    uint32_t utf32 = 0;
    const uint32_t letterUtf32be = 0x41000000u;
    const uint32_t letterUtf32le = 0x00000041u;
    const uint16_t letterUtf16be = 0x4100u;
    const uint16_t letterUtf16le = 0x0041u;
    const uint16_t letterUtf16 = 'A';
    const uint8_t unassignedSjis[] = {0x85, 0x40};
    constexpr int ERR_UNASSIGNED_CODE = static_cast<int>(0x805C0020);
    const auto faults = [&](int result) {
        const bool kept = errno == savedErrno;
        errno = savedErrno;
        return result == ERR_INVALID_DST_BUFFER && kept;
    };

    errno = savedErrno;
    Require(faults(sceCesEucKrToUtf8(eucKr, ascii, 1, nullptr, unmappedU8, 4, nullptr)));
    Require(faults(sceCesEucKrToUtf8(eucKr, ascii, 1, unmappedU32, utf8, 4, nullptr)));
    Require(faults(sceCesEucKrToUtf8(eucKr, ascii, 1, nullptr, utf8, 4, unmappedU32)));
    Require(faults(sceCesSJisToUcs2(sjis, ascii, 1, nullptr, unmappedU16)));
    Require(faults(sceCesSJisToUcs2(sjis, ascii, 1, unmappedU32, &unit)));
    Require(faults(sceCesSJisToUtf8(sjis, ascii, 1, nullptr, unmappedU8, 4, nullptr)));
    Require(faults(sceCesSJisToUtf8(sjis, ascii, 1, nullptr, utf8, 4, unmappedU32)));
    Require(faults(sceCesSJisToUtf16(sjis, ascii, 1, nullptr, unmappedU16, 2, nullptr)));
    Require(faults(sceCesSJisToUtf16(sjis, ascii, 1, nullptr, utf16, 2, unmappedU32)));
    Require(faults(sceCesSJisToUtf32(sjis, ascii, 1, nullptr, unmappedU32)));
    Require(faults(sceCesSJisToUtf32(sjis, ascii, 1, unmappedU32, &utf32)));
    Require(faults(sceCesUtf8ToSJis(ascii, 1, nullptr, sjis, unmappedU8, 2, nullptr)));
    Require(faults(sceCesUtf8ToSJis(ascii, 1, nullptr, sjis, utf8, 2, unmappedU32)));
    Require(faults(sceCesUtf8ToSJis(ascii, 1, unmappedU32, sjis, utf8, 2, &size)));
    Require(faults(sceCesUtf32beToSJis(&letterUtf32be, sjis, unmappedU8, 2, nullptr)));
    Require(faults(sceCesUtf32beToSJis(&letterUtf32be, sjis, utf8, 2, unmappedU32)));
    Require(faults(sceCesSbcToUtf16be(cp1252, 'A', unmappedU16, 1, nullptr)));
    Require(faults(sceCesSbcToUtf16be(cp1252, 'A', utf16, 1, unmappedU32)));
    Require(faults(sceCesSbcToUtf32be(cp1252, 'A', unmappedU32)));
    Require(faults(sceCesUtf16beToSbc(&letterUtf16be, 1, nullptr, cp1252, unmappedU8)));
    Require(faults(sceCesUtf16beToSbc(&letterUtf16be, 1, unmappedU32, cp1252, &byte)));
    Require(faults(sceCesUtf32beToSbc(&letterUtf32be, cp1252, unmappedU8)));
    Require(faults(sceCesUcs2ToUtf8('A', unmappedU8, 4, nullptr)));
    Require(faults(sceCesUcs2ToUtf8('A', utf8, 4, unmappedU32)));
    Require(faults(sceCesUtf8ToUtf32(ascii, 1, nullptr, unmappedU32)));
    Require(faults(sceCesUtf8ToUtf32(ascii, 1, unmappedU32, &utf32)));
    Require(faults(sceCesUtf8ToUtf32be(ascii, 1, nullptr, unmappedU32)));
    Require(faults(sceCesUtf8ToUtf32be(ascii, 1, unmappedU32, &utf32)));
    Require(faults(sceCesUtf8ToUtf32le(ascii, 1, nullptr, unmappedU32)));
    Require(faults(sceCesUtf8ToUtf32le(ascii, 1, unmappedU32, &utf32)));
    Require(faults(sceCesUtf8ToUtf16(ascii, 1, nullptr, unmappedU16, 2, nullptr)));
    Require(faults(sceCesUtf8ToUtf16(ascii, 1, unmappedU32, utf16, 2, nullptr)));
    Require(faults(sceCesUtf8ToUtf16(ascii, 1, nullptr, utf16, 2, unmappedU32)));
    Require(faults(sceCesUtf8ToUtf16be(ascii, 1, nullptr, unmappedU16, 2, nullptr)));
    Require(faults(sceCesUtf8ToUtf16be(ascii, 1, unmappedU32, utf16, 2, nullptr)));
    Require(faults(sceCesUtf8ToUtf16be(ascii, 1, nullptr, utf16, 2, unmappedU32)));
    Require(faults(sceCesUtf8ToUtf16le(ascii, 1, nullptr, unmappedU16, 2, nullptr)));
    Require(faults(sceCesUtf8ToUtf16le(ascii, 1, unmappedU32, utf16, 2, nullptr)));
    Require(faults(sceCesUtf8ToUtf16le(ascii, 1, nullptr, utf16, 2, unmappedU32)));
    Require(faults(sceCesUtf16ToUtf8(&letterUtf16, 1, nullptr, unmappedU8, 4, nullptr)));
    Require(faults(sceCesUtf16ToUtf8(&letterUtf16, 1, unmappedU32, utf8, 4, nullptr)));
    Require(faults(sceCesUtf16ToUtf8(&letterUtf16, 1, nullptr, utf8, 4, unmappedU32)));
    Require(faults(sceCesUtf16beToUtf8(&letterUtf16be, 1, nullptr, unmappedU8, 4, nullptr)));
    Require(faults(sceCesUtf16beToUtf8(&letterUtf16be, 1, unmappedU32, utf8, 4, nullptr)));
    Require(faults(sceCesUtf16beToUtf8(&letterUtf16be, 1, nullptr, utf8, 4, unmappedU32)));
    Require(faults(sceCesUtf16leToUtf8(&letterUtf16le, 1, nullptr, unmappedU8, 4, nullptr)));
    Require(faults(sceCesUtf16leToUtf8(&letterUtf16le, 1, unmappedU32, utf8, 4, nullptr)));
    Require(faults(sceCesUtf16leToUtf8(&letterUtf16le, 1, nullptr, utf8, 4, unmappedU32)));
    Require(faults(sceCesUtf32ToUtf8('A', unmappedU8, 4, nullptr)));
    Require(faults(sceCesUtf32ToUtf8('A', utf8, 4, unmappedU32)));
    Require(faults(sceCesUtf32beToUtf8(&letterUtf32be, unmappedU8, 4, nullptr)));
    Require(faults(sceCesUtf32beToUtf8(&letterUtf32be, utf8, 4, unmappedU32)));
    Require(faults(sceCesUtf32leToUtf8(&letterUtf32le, unmappedU8, 4, nullptr)));
    Require(faults(sceCesUtf32leToUtf8(&letterUtf32le, utf8, 4, unmappedU32)));
    Require(sceCesSJisToUcs2(sjis, unassignedSjis, 2, &size, &unit) == ERR_UNASSIGNED_CODE && size == 2);
    errno = savedErrno;
    Require(faults(sceCesSJisToUcs2(sjis, unassignedSjis, 2, unmappedU32, &unit)));
    Require(sceCesEucKrToUtf8(eucKr, ascii, 1, &size, utf8, 4, &utf32) == 0 && utf8[0] == 'A' && size == 1 && utf32 == 1);
    Require(sceCesSJisToUtf16(sjis, ascii, 1, &size, utf16, 2, &utf32) == 0 && utf16[0] == 'A' && size == 1 && utf32 == 1);
    Require(sceCesUtf32beToSbc(&letterUtf32be, cp1252, &byte) == 0 && byte == 'A');

    alignas(8) uint8_t eucJpSheet[64] = {};
    const void* eucJp = sceCesUcsProfileInitEucJpCp51932(eucJpSheet);
    Require(eucJp != nullptr);
    const auto* const unreadable = reinterpret_cast<const uint8_t*>(0x10);
    const auto unreadableSource = [&](int result) {
        const bool kept = errno == savedErrno;
        errno = savedErrno;
        return result == ERR_INVALID_SRC_BUFFER && kept;
    };
    errno = savedErrno;
    Require(unreadableSource(sceCesEucKrToUtf8(eucKr, unreadable, 1, &size, utf8, 4, nullptr)));
    Require(unreadableSource(sceCesSJisToUcs2(sjis, unreadable, 2, &size, &unit)));
    Require(unreadableSource(sceCesSJisToUtf8(sjis, unreadable, 2, &size, utf8, 4, nullptr)));
    Require(unreadableSource(sceCesUtf8ToSJis(unreadable, 1, nullptr, sjis, utf8, 2, nullptr)));
    Require(unreadableSource(sceCesUtf32beToSJis(reinterpret_cast<const uint32_t*>(0x10), sjis, utf8, 2, nullptr)));
    Require(unreadableSource(sceCesUtf16beToSbc(reinterpret_cast<const uint16_t*>(0x10), 1, nullptr, cp1252, &byte)));
    Require(unreadableSource(sceCesUtf32beToSbc(reinterpret_cast<const uint32_t*>(0x10), cp1252, &byte)));
    Require(unreadableSource(sceCesEucJpToUcs2(eucJp, unreadable, 2, &size, &unit)));
    Require(unreadableSource(sceCesEucJpToUtf8(eucJp, unreadable, 2, &size, utf8, 4, nullptr)));
    Require(unreadableSource(sceCesUtf8ToSbc(unreadable, 1, nullptr, cp1252, &byte)));
    Require(unreadableSource(sceCesUtf8ToUtf32(unreadable, 1, nullptr, &utf32)));
    Require(unreadableSource(sceCesUtf8ToUtf32be(unreadable, 1, nullptr, &utf32)));
    Require(unreadableSource(sceCesUtf8ToUtf32le(unreadable, 1, nullptr, &utf32)));
    Require(unreadableSource(sceCesUtf8ToUtf16(unreadable, 1, nullptr, utf16, 2, nullptr)));
    Require(unreadableSource(sceCesUtf8ToUtf16be(unreadable, 1, nullptr, utf16, 2, nullptr)));
    Require(unreadableSource(sceCesUtf8ToUtf16le(unreadable, 1, nullptr, utf16, 2, nullptr)));
    Require(unreadableSource(sceCesUtf8ToUcs2(unreadable, 1, nullptr, &unit)));
    Require(unreadableSource(sceCesUtf16ToUtf8(reinterpret_cast<const uint16_t*>(0x10), 1, nullptr, utf8, 4, nullptr)));
    Require(unreadableSource(sceCesUtf16beToUtf8(reinterpret_cast<const uint16_t*>(0x10), 1, nullptr, utf8, 4, nullptr)));
    Require(unreadableSource(sceCesUtf16leToUtf8(reinterpret_cast<const uint16_t*>(0x10), 1, nullptr, utf8, 4, nullptr)));
    Require(unreadableSource(sceCesUtf32beToUtf8(reinterpret_cast<const uint32_t*>(0x10), utf8, 4, nullptr)));
    Require(unreadableSource(sceCesUtf32leToUtf8(reinterpret_cast<const uint32_t*>(0x10), utf8, 4, nullptr)));
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const auto page = static_cast<std::size_t>(system.dwPageSize);
    auto* pages = static_cast<uint8_t*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD previous = 0;
    Require(pages != nullptr && VirtualProtect(pages + page, page, PAGE_NOACCESS, &previous) != 0);
    uint8_t* const last = pages + page - 1;
    *last = 'A';
    Require(sceCesSJisToUcs2(sjis, last, 4, &size, &unit) == 0 && unit == 'A' && size == 1);
    Require(sceCesUtf8ToUtf32(last, 1, &size, &utf32) == 0 && utf32 == 'A' && size == 1);
    *last = 0x82;
    errno = savedErrno;
    Require(unreadableSource(sceCesSJisToUcs2(sjis, last, 2, &size, &unit)));
    *last = 0xC2;
    errno = savedErrno;
    Require(unreadableSource(sceCesUtf8ToUtf32(last, 2, &size, &utf32)));
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#elif defined(__linux__)
    const long page = sysconf(_SC_PAGESIZE);
    auto* pages = static_cast<uint8_t*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED && mprotect(pages + page, page, PROT_NONE) == 0);
    uint8_t* const last = pages + page - 1;
    *last = 'A';
    Require(sceCesSJisToUcs2(sjis, last, 4, &size, &unit) == 0 && unit == 'A' && size == 1);
    Require(sceCesUtf8ToUtf32(last, 1, &size, &utf32) == 0 && utf32 == 'A' && size == 1);
    *last = 0x82;
    errno = savedErrno;
    Require(unreadableSource(sceCesSJisToUcs2(sjis, last, 2, &size, &unit)));
    *last = 0xC2;
    errno = savedErrno;
    Require(unreadableSource(sceCesUtf8ToUtf32(last, 2, &size, &utf32)));
    Require(munmap(pages, page * 2) == 0);
#endif

    std::puts("guest_ces_guest_pointer_faults ok");
    return 0;
}
