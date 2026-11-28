#include "prx/libSceMusicFrameworkInterface/libSceMusicFrameworkInterface.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int invalidArgument = static_cast<int>(0x81797001u);

}

int main() {
    using Getter = decltype(&sceMusicFwGetVolume);
    const std::array<Getter, 14> getters{
        sceMusicFwGetCurrentTime, sceMusicFwGetLoop, sceMusicFwGetMuted, sceMusicFwGetNetworkState, sceMusicFwGetVolume,
        sceMusicFwIsEnded, sceMusicFwIsPlaying, sceMusicFwIsStopped, sceMusicFwGetCapability, sceMusicFwGetDuration,
        sceMusicFwGetShuffle, sceMusicFwIsPaused, sceMusicFwIsSeeking, libSceMusicFrameworkInterfaceUnknown_X9GN6gWabWM_nid_no_patch};
    for (const Getter getter : getters) {
        for (const std::int32_t handle : std::array<std::int32_t, 4>{0, 1, -1, 0x7fffffff})
            Require(getter(handle, nullptr) == invalidArgument);
    }
    Require(sceMusicFwGetVolume2(nullptr) == invalidArgument);
    Require(sceMusicFwInitialize2(nullptr) == invalidArgument);
    return 0;
}
