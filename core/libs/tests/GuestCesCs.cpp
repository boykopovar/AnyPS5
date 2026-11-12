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
    return 0;
}
