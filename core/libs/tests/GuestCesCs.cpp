#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int invalidProfile = static_cast<int>(0x805C0004);

void Require(bool value) { if (!value) std::abort(); }

struct Sheet {
    alignas(8) std::uint8_t bytes[64];
};

}

int main() {
    Sheet isoSheet{};
    std::memset(isoSheet.bytes, 0, sizeof(isoSheet.bytes));
    Require(sceCesUcsProfileInitIso2022JpRfc1468(isoSheet.bytes) == isoSheet.bytes);

    errno = 0x5a;
    Require(sceCesIso2022JpUcsProfileSetSbcs(nullptr, sceCesRefersUcsProfileAscii()) == invalidProfile && errno == 0x5a);
    Require(sceCesIso2022JpUcsProfileSetSbcs(isoSheet.bytes, nullptr) == invalidProfile && errno == 0x5a);
    Require(sceCesIso2022JpUcsProfileSetSbcs(isoSheet.bytes, sceCesRefersUcsProfileCp1252()) == invalidProfile && errno == 0x5a);

    Sheet nonIsoSheet{};
    std::memset(nonIsoSheet.bytes, 0, sizeof(nonIsoSheet.bytes));
    Require(sceCesIso2022JpUcsProfileSetSbcs(nonIsoSheet.bytes, sceCesRefersUcsProfileAscii()) == invalidProfile && errno == 0x5a);

    Require(sceCesIso2022JpUcsProfileSetSbcs(isoSheet.bytes, sceCesRefersUcsProfileAscii()) == 0 && errno == 0x5a);
    Require(sceCesIso2022JpUcsProfileSetSbcs(isoSheet.bytes, sceCesRefersUcsProfileJisX0201Roman()) == 0 && errno == 0x5a);
    Require(sceCesIso2022JpUcsProfileSetSbcs(isoSheet.bytes, sceCesRefersUcsProfileJisX0201()) == 0 && errno == 0x5a);

    Sheet gbSheet{};
    std::memset(gbSheet.bytes, 0, sizeof(gbSheet.bytes));
    errno = 0x5a;
    Require(sceCesUcsProfileInitGb18030_2022(nullptr) == nullptr && errno == 0x5a);
    Require(sceCesUcsProfileInitGb18030_2022(gbSheet.bytes) == gbSheet.bytes && errno == 0x5a);
    Require(sceCesGbUcsProfileSet4ByteCharRange(nullptr, 1) == invalidProfile && errno == 0x5a);
    Require(sceCesGbUcsProfileSet4ByteCharRange(gbSheet.bytes, 1) == 0 && errno == 0x5a);
    Require(sceCesGbUcsProfileSetSbcs(nullptr, sceCesRefersUcsProfileGbT1988()) == invalidProfile && errno == 0x5a);
    Require(sceCesGbUcsProfileSetSbcs(gbSheet.bytes, nullptr) == invalidProfile && errno == 0x5a);
    Require(sceCesGbUcsProfileSetSbcs(gbSheet.bytes, sceCesRefersUcsProfileCp1252()) == invalidProfile && errno == 0x5a);
    Require(sceCesGbUcsProfileSetSbcs(isoSheet.bytes, sceCesRefersUcsProfileGbT1988()) == invalidProfile && errno == 0x5a);
    Require(sceCesGbUcsProfileSetSbcs(gbSheet.bytes, sceCesRefersUcsProfileAscii()) == 0 && errno == 0x5a);
    Require(sceCesGbUcsProfileSetSbcs(gbSheet.bytes, sceCesRefersUcsProfileGbT1988()) == 0 && errno == 0x5a);

    constexpr int invalidParameter = static_cast<int>(0x805C0001);
    auto* badPtr = reinterpret_cast<void*>(0x10);
    std::uint8_t mbcsContext[256]{};
    std::uint8_t isoContext[256]{};
    errno = 0x5a;
    Require(sceCesUcsProfileInitSJis1997Cp932(badPtr) == nullptr && errno == 0x5a);
    Require(sceCesUcsProfileInitEucJpCp51932(badPtr) == nullptr && errno == 0x5a);
    Require(sceCesUcsProfileInitEucKr(badPtr) == nullptr && errno == 0x5a);
    Require(sceCesUcsProfileInitUhc(badPtr) == nullptr && errno == 0x5a);
    Require(sceCesUcsProfileInitBig5Cp950(badPtr) == nullptr && errno == 0x5a);
    Require(sceCesUcsProfileInitEucCnGb2312(badPtr) == nullptr && errno == 0x5a);
    Require(sceCesUcsProfileInitGbkCp936(badPtr) == nullptr && errno == 0x5a);
    Require(sceCesUcsProfileInitGb18030_2022(badPtr) == nullptr && errno == 0x5a);
    Require(sceCesUcsProfileInitIso2022JpRfc1468(badPtr) == nullptr && errno == 0x5a);
    Require(sceCesIso2022JpUcsProfileSetSbcs(badPtr, sceCesRefersUcsProfileAscii()) == invalidProfile && errno == 0x5a);
    Require(sceCesGbUcsProfileSetSbcs(badPtr, sceCesRefersUcsProfileAscii()) == invalidProfile && errno == 0x5a);
    Require(sceCesGbUcsProfileSet4ByteCharRange(badPtr, 1) == invalidProfile && errno == 0x5a);
    Require(sceCesGbUcsProfileSetUdaMapping(badPtr, 1) == invalidProfile && errno == 0x5a);
    Require(sceCesMbcsUcsContextInit(badPtr, gbSheet.bytes) == invalidParameter && errno == 0x5a);
    Require(sceCesMbcsUcsContextInit(mbcsContext, gbSheet.bytes) == 0 && errno == 0x5a);
    Require(sceCesMbcsUcsContextInitCopy(badPtr, mbcsContext) == invalidParameter && errno == 0x5a);
    Require(sceCesMbcsUcsContextInitCopy(mbcsContext, badPtr) == invalidParameter && errno == 0x5a);
    Require(sceCesIso2022UcsContextInit(badPtr, isoSheet.bytes) == invalidParameter && errno == 0x5a);
    Require(sceCesIso2022UcsContextInit(isoContext, isoSheet.bytes) == 0 && errno == 0x5a);
    Require(sceCesIso2022UcsContextInitCopy(badPtr, isoContext) == invalidParameter && errno == 0x5a);
    Require(sceCesIso2022UcsContextInitCopy(isoContext, badPtr) == invalidParameter && errno == 0x5a);
    return 0;
}
