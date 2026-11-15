#include "prx/libSceRemotePlayClientIpc/libSceRemotePlayClientIpc.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int notConnected = static_cast<int>(0x8122101Du);

}

int main() {
    for (int round = 0; round < 2; ++round) {
        Require(sceRemotePlayClientIpcGetClientStatus() == notConnected);
        Require(sceRemotePlayClientIpcSetClientStatus() == notConnected);
        Require(sceRemotePlayClientIpcTerminateStreaming() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_0Ezd4jv7F_minus_4_nid_no_patch() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_4_plus_sKR9yWkWY_nid_no_patch() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_EVbYn8UOvMs_nid_no_patch() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_JGKtr0_plus_B87A_nid_no_patch() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_JVKO2gPlWPw_nid_no_patch() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_M16_minus_T2ERA8c_nid_no_patch() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_PnLBwmAGb_minus_g_nid_no_patch() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_RiyUwZBmS_minus_k_nid_no_patch() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_gO3jwkOQ834_nid_no_patch() == notConnected);
        Require(libSceRemotePlayClientIpcUnknown_gqmq4cTnR0M_nid_no_patch() == notConnected);
    }
    return 0;
}
