#include "prx/libScePngParser/libScePngParser.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    bool threwUnmapped = false;
    errno = 71;
    try {
        static_cast<void>(libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(reinterpret_cast<const void*>(0x10)));
    } catch (const std::invalid_argument&) {
        threwUnmapped = true;
    }
    Require(threwUnmapped && errno == 71);

    std::array<unsigned char, 0x58> config{};
    for (std::size_t i = 0; i < config.size(); ++i) config[i] = static_cast<unsigned char>(i + 1);
    for (int call = 0; call < 2; ++call) {
        Require(libScePngParserUnknown_0Imz9zU_minus_Wgk_nid_no_patch() == static_cast<int>(0x809D4019u));
        Require(libScePngParserUnknown_5XfcHLzGE6E_nid_no_patch() == static_cast<int>(0x809D4054u));
        errno = 71;
        Require(libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(config.data()) == 0);
        Require(errno == 71);
        Require(_ZN23sceMetadataReaderWriter9pngParser17pngParserFinalizeEv() == 0);
    }
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
