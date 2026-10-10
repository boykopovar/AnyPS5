#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstring>

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

int APS5_VABI sceGameLiveStreamingInitialize(size_t heap_size) {
    if (heap_size == 0) APS5_INVALID_ARG_EX;
    return 0;
}

int APS5_VABI sceGameLiveStreamingTerminate(void) {
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetCurrentStatus2(GameLiveStreamingStatus2* status) {
    if (status == nullptr) APS5_INVALID_ARG_EX;
    std::memset(status, 0, sizeof(*status));
    status->userId = USER_SERVICE_USER_ID_INVALID;
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetProgramInfo() {
    throw std::runtime_error("sceGameLiveStreamingGetProgramInfo: unknown signature");
}

}
