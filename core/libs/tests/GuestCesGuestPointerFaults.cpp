#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

    std::puts("guest_ces_guest_pointer_faults ok");
    return 0;
}
