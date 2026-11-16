#include "prx/libSceGifParser/libSceGifParser.hpp"
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
    constexpr int unsupported = static_cast<int>(0x809D4054u);
    Require(_ZN23sceMetadataReaderWriter9gifParser17gifParserFinalizeEv() == 0);
    Require(_ZN23sceMetadataReaderWriter9gifParser17gifParserFinalizeEv() == 0);

    bool threwNull = false;
    try {
        static_cast<void>(libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(nullptr));
    } catch (const std::invalid_argument&) {
        threwNull = true;
    }
    Require(threwNull);

    bool threwUnmapped = false;
    errno = 71;
    try {
        static_cast<void>(libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(reinterpret_cast<const void*>(0x10)));
    } catch (const std::invalid_argument&) {
        threwUnmapped = true;
    }
    Require(threwUnmapped && errno == 71);

    std::array<unsigned char, 0x58> config{};
    for (std::size_t i = 0; i < config.size(); ++i) config[i] = static_cast<unsigned char>(i + 1);
    const auto snapshot = config;
    for (int call = 0; call < 2; ++call) {
        Require(libSceGifParserUnknown_qiEL_minus_fPgaAA_nid_no_patch() == unsupported);
        Require(libSceGifParserUnknown__minus_ZzjSDCInl4_nid_no_patch() == unsupported);
        errno = 71;
        Require(libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(config.data()) == 0);
        Require(errno == 71);
        Require(config == snapshot);
        Require(_ZN23sceMetadataReaderWriter9gifParser17gifParserFinalizeEv() == 0);
    }
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
