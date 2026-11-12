#include "prx/libSceCdlgUtilServer/libSceCdlgUtilServer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

std::array<unsigned char, 32> NullServer() {
    std::array<unsigned char, 32> object{};
    object.fill(0x5A);
    for (std::size_t i = 0x8; i < 0x10; ++i) object[i] = 0;
    return object;
}

void RequireUnchanged(const std::array<unsigned char, 32>& object) {
    const std::array<unsigned char, 32> expected = NullServer();
    Require(object == expected);
}

void CheckNullServer() {
    constexpr int result80B8000E = static_cast<int>(0x80B8000Eu);
    for (int call = 0; call < 2; ++call) {
        auto object = NullServer();
        unsigned char data[4] = {1, 2, 3, 4};
        Require(_ZN3sce16CommonDialogUtil6Server13setFinishDataEPvm(object.data(), data, sizeof(data)) == result80B8000E);
        Require(_ZN3sce16CommonDialogUtil6Server6setRunEPvm(object.data(), data, sizeof(data)) == result80B8000E);
        Require(_ZN3sce16CommonDialogUtil6Server9setFinishEv(object.data()) == result80B8000E);
        Require(_ZN3sce16CommonDialogUtil6Server9setResultEi(object.data(), 7) == result80B8000E);
        std::uint32_t version = 0xCAFEF00Du;
        Require(_ZNK3sce16CommonDialogUtil6Server10getVersionERj(object.data(), &version) == result80B8000E);
        Require(version == 0xCAFEF00Du);
        Require(_ZNK3sce16CommonDialogUtil6Server10isCloseReqEv(object.data()) == 0);
        Require(_ZNK3sce16CommonDialogUtil6Server14getServerStateEv(object.data()) == -1);
        Require(libSceCdlgUtilServerUnknown_lhSjnuF2bzg_nid_no_patch(object.data()) == 0);
        Require(libSceCdlgUtilServerUnknown_j6vGxXgVnCQ_nid_no_patch(object.data()) == result80B8000E);
        Require(libSceCdlgUtilServerUnknown_ZRy0WQp2COs_nid_no_patch(object.data()) == result80B8000E);
        Require(libSceCdlgUtilServerUnknown_aztojjv0xT0_nid_no_patch(object.data()) == result80B8000E);
        Require(libSceCdlgUtilServerUnknown_dQte4n_minus_nQlg_nid_no_patch(object.data()) == 0);
        Require(libSceCdlgUtilServerUnknown_VrN0lR_plus_BJyc_nid_no_patch(object.data()) == result80B8000E);
        Require(libSceCdlgUtilServerUnknown_1IzIqsh2lVE_nid_no_patch(object.data()) == -1);
        Require(libSceCdlgUtilServerUnknown_qFygU3yA0HE_nid_no_patch(object.data()) == 0xFFFFu);
        Require(libSceCdlgUtilServerUnknown_NKhtpQTUjpY_nid_no_patch(object.data()) == -1);
        Require(libSceCdlgUtilServerUnknown_vfUweaM01gw_nid_no_patch(object.data()) == -1);
        Require(data[0] == 1 && data[1] == 2 && data[2] == 3 && data[3] == 4);
        RequireUnchanged(object);
    }
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
    CheckNullServer();
    return 0;
}
