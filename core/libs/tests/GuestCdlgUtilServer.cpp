#include "prx/libSceCdlgUtilServer/libSceCdlgUtilServer.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    for (int call = 0; call < 2; ++call) Require(libSceCdlgUtilServerUnknown_aFrwE9W9faU_nid_no_patch() == 0);

    for (const unsigned char value : {0x00, 0x01, 0x7F, 0x80, 0xC3, 0xFF}) {
        std::array<unsigned char, 32> object{};
        object.fill(0x5A);
        object[0x10] = value;
        for (int call = 0; call < 2; ++call) Require(libSceCdlgUtilServerUnknown_W4zcizD4i3I_nid_no_patch(object.data()) == value);
        for (std::size_t i = 0; i < object.size(); ++i) Require(object[i] == (i == 0x10 ? value : 0x5A));
    }
    return 0;
}
