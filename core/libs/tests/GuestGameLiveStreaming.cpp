#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

struct GameLiveStreamingStatus2 {
    std::int32_t userId;
    bool isOnAir;
    std::uint8_t align[3];
    std::uint32_t spectatorCounts;
    std::uint32_t textMessageCounts;
    std::uint32_t commandMessageCounts;
    std::uint32_t broadcastVideoResolution;
    std::uint8_t reserved[48];
};
static_assert(sizeof(GameLiveStreamingStatus2) == 72);

extern "C" {
int APS5_VABI sceGameLiveStreamingInitialize(std::size_t heap_size);
int APS5_VABI sceGameLiveStreamingTerminate(void);
int APS5_VABI sceGameLiveStreamingGetCurrentStatus2(GameLiveStreamingStatus2* status);
int APS5_VABI sceGameLiveStreamingGetProgramInfo(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::int32_t kInvalidUserId = -1;

}

int main() {
    bool threwZeroHeap = false;
    try {
        sceGameLiveStreamingInitialize(0);
    } catch (const std::invalid_argument&) {
        threwZeroHeap = true;
    }
    Require(threwZeroHeap);

    Require(sceGameLiveStreamingInitialize(1024 * 1024) == 0);

    bool threwNullStatus = false;
    try {
        sceGameLiveStreamingGetCurrentStatus2(nullptr);
    } catch (const std::invalid_argument&) {
        threwNullStatus = true;
    }
    Require(threwNullStatus);

    GameLiveStreamingStatus2 status{};
    std::memset(&status, 0x7f, sizeof(status));
    Require(sceGameLiveStreamingGetCurrentStatus2(&status) == 0);
    Require(status.userId == kInvalidUserId);
    Require(!status.isOnAir);
    Require(status.spectatorCounts == 0);
    Require(status.textMessageCounts == 0);
    Require(status.commandMessageCounts == 0);
    Require(status.broadcastVideoResolution == 0);
    for (std::uint8_t byte : status.align) {
        Require(byte == 0);
    }
    for (std::uint8_t byte : status.reserved) {
        Require(byte == 0);
    }

    bool threwProgramInfo = false;
    try {
        sceGameLiveStreamingGetProgramInfo();
    } catch (const std::runtime_error&) {
        threwProgramInfo = true;
    }
    Require(threwProgramInfo);

    Require(sceGameLiveStreamingTerminate() == 0);
}
