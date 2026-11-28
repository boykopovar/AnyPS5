#include "prx/libSceIcu/include/Availability.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "ucmndata.h"
#include "udataswp.h"
#include "uresdata.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

struct SingleResourcePackage {
    DataHeader header;
    std::uint32_t count;
    std::uint32_t reserved;
    const char* name;
    const void* resource;
};

struct DirectionCase {
    const char* locale;
    bool rtl;
};

}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    static std::vector<char> resource{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (resource.empty()) {
        std::fprintf(stderr, "Could not read %s\n", argv[1]);
        return 1;
    }
    const auto length = static_cast<std::int32_t>(resource.size());
    UErrorCode status = U_ZERO_ERROR;
    auto* swapper = udata_openSwapperForInputData(resource.data(), length, U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, &status);
    if (U_SUCCESS(status)) {
        ures_swap(swapper, resource.data(), length, resource.data(), &status);
        udata_closeSwapper(swapper);
    }
    if (U_FAILURE(status)) {
        std::fprintf(stderr, "Could not swap langInfo.res: %s\n", u_errorName(status));
        return 1;
    }
    alignas(16) static const SingleResourcePackage package{
        {{sizeof(DataHeader), 0xda, 0x27}, {sizeof(UDataInfo), 0, U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, U_SIZEOF_UCHAR,
          0, {'T', 'o', 'C', 'P'}, {1, 0, 0, 0}, {}}},
        1, 0, U_ICUDATA_NAME "/langInfo.res", resource.data()
    };
    std::int32_t guestStatus = 0;
    udata_setCommonData_nid_postfix(&package, &guestStatus);
    if (guestStatus != 0) {
        std::fprintf(stderr, "Could not register langInfo.res: %d\n", guestStatus);
        return 1;
    }

    constexpr DirectionCase cases[] = {
        {"ur_PK", true}, {"dv", true}, {"yi", true}, {"pa_PK", true}, {"pa_IN", false},
        {"az_IR", true}, {"az_AZ", false}, {"und_IL", true}, {"und_US", false}, {"ur_Latn", false}
    };
    for (const auto& [locale, rtl] : cases) {
        const auto actual = uloc_isRightToLeft_nid_postfix(locale);
        if (actual != rtl) {
            std::fprintf(stderr, "%s: expected direction %d, got %d\n", locale, rtl, actual);
            return 1;
        }
    }
    return 0;
}
