#include "prx/libSceGifParser/libSceGifParser.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    constexpr int unsupported = static_cast<int>(0x809D4054u);
    std::array<unsigned char, 0x58> config{};
    for (std::size_t i = 0; i < config.size(); ++i) config[i] = static_cast<unsigned char>(i + 1);
    for (int call = 0; call < 2; ++call) {
        Require(libSceGifParserUnknown_qiEL_minus_fPgaAA_nid_no_patch() == unsupported);
        Require(libSceGifParserUnknown__minus_ZzjSDCInl4_nid_no_patch() == unsupported);
        Require(libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(config.data()) == 0);
        Require(libSceGifParserUnknown_gtaHM_minus_0DRuA_nid_no_patch() == 0);
    }
    return 0;
}
