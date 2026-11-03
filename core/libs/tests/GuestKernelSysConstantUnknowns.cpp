#include "prx/libkernel_sys/include/AdditionalExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    Require(libkernel_sysUnknown_pCbBddNl66Y_nid_no_patch() == 0, "pCbBddNl66Y returns 0");
    Require(libkernel_sysUnknown_zAloppPJxy0_nid_no_patch() == 0, "zAloppPJxy0 returns 0");
    Require(libkernel_sysUnknown_vq2_plus_HyP5U5I_nid_no_patch() == 0, "vq2+HyP5U5I returns 0");
    Require(libkernel_sysUnknown_z_plus_e0vV_minus_tnH4_nid_no_patch() == 0, "z+e0vV-tnH4 returns 0");
    Require(libkernel_sysUnknown_gJ5gpW45mU0_nid_no_patch() == 0, "gJ5gpW45mU0 returns 0");
    Require(libkernel_sysUnknown_N94a0T4F4_plus_U_nid_no_patch() == 0x8002004eu, "N94a0T4F4+U returns 0x8002004e zero-extended");
    Require(libkernel_sysUnknown_C49jelxiaVE_nid_no_patch() == 0x20018u, "C49jelxiaVE returns 0x20018");
    Require(libkernel_sysUnknown_FR0odl35yQY_nid_no_patch() == 0, "FR0odl35yQY reads 0 before the flag is set");
    Require(libkernel_sysUnknown_1WXQlxMpnNA_nid_no_patch() == 0, "1WXQlxMpnNA returns 0");
    Require(libkernel_sysUnknown_FR0odl35yQY_nid_no_patch() == 1, "FR0odl35yQY reads 1 after 1WXQlxMpnNA");
    Require(libkernel_sysUnknown_1WXQlxMpnNA_nid_no_patch() == 0, "a second 1WXQlxMpnNA returns 0");
    Require(libkernel_sysUnknown_FR0odl35yQY_nid_no_patch() == 1, "the flag stays set");
}
