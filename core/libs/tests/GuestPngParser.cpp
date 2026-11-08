#include "prx/libScePngParser/libScePngParser.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    std::array<unsigned char, 0x58> config{};
    for (std::size_t i = 0; i < config.size(); ++i) config[i] = static_cast<unsigned char>(i + 1);
    for (int call = 0; call < 2; ++call) {
        Require(libScePngParserUnknown_0Imz9zU_minus_Wgk_nid_no_patch() == static_cast<int>(0x809D4019u));
        Require(libScePngParserUnknown_5XfcHLzGE6E_nid_no_patch() == static_cast<int>(0x809D4054u));
        Require(libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(config.data()) == 0);
        Require(libScePngParserUnknown_suBT1aYALqA_nid_no_patch() == 0);
    }
    return 0;
}
