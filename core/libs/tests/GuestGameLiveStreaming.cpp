#include "prx/libSceGameLiveStreaming/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <span>

static_assert(sizeof(GameLiveStreamingStatus2) == 72);

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::int32_t kInvalidUserId = -1;

bool EnableThrowsNotInitialized(bool isEnableLiveStreaming) {
    try {
        sceGameLiveStreamingEnableLiveStreaming(isEnableLiveStreaming);

    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

}

int main() {
    constexpr int invalidArgument = static_cast<int>(0x80A00002);
    constexpr int alreadyInitialized = static_cast<int>(0x80A00003);
    constexpr int notInitialized = static_cast<int>(0x80A00004);
    Require(sceGameLiveStreamingTerminate() == notInitialized);
    Require(sceGameLiveStreamingGetCurrentStatus2(nullptr) == notInitialized);
    Require(EnableThrowsNotInitialized(true));
    for (const std::size_t size : {0u, 1u, 0x3fffu, 0x4001u, 0x100000u}) Require(sceGameLiveStreamingInitialize(size) == invalidArgument);
    Require(sceGameLiveStreamingInitialize(0x4000) == 0);
    Require(sceGameLiveStreamingEnableLiveStreaming(true) == 0);
    Require(sceGameLiveStreamingInitialize(0) == invalidArgument);
    Require(sceGameLiveStreamingInitialize(0x4000) == alreadyInitialized);
    Require(sceGameLiveStreamingGetCurrentStatus2(nullptr) == invalidArgument);

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

    Require(sceGameLiveStreamingGetProgramInfo(nullptr) == static_cast<int>(0x80A00002));
    std::array<std::uint8_t, 0x134C + 16> programInfo{};
    programInfo.fill(0x7f);
    Require(sceGameLiveStreamingGetProgramInfo(reinterpret_cast<GameLiveStreamingProgramInfo*>(programInfo.data())) == 0);
    for (std::size_t i = 0; i < programInfo.size(); ++i) {
        Require(programInfo[i] == (i < 0x134C ? 0 : 0x7f));
    }

    Require(sceGameLiveStreamingEnableLiveStreaming(false) == 0);
    Require(sceGameLiveStreamingTerminate() == 0);
    Require(EnableThrowsNotInitialized(false));
    std::memset(&status, 0x7f, sizeof(status));
    Require(sceGameLiveStreamingGetCurrentStatus2(&status) == notInitialized);
    for (const auto value : std::as_bytes(std::span(&status, 1))) Require(value == std::byte{0x7f});
    programInfo.fill(0x7f);
    Require(sceGameLiveStreamingGetProgramInfo(reinterpret_cast<GameLiveStreamingProgramInfo*>(programInfo.data())) == notInitialized);
    for (const std::uint8_t byte : programInfo) Require(byte == 0x7f);
    Require(sceGameLiveStreamingTerminate() == notInitialized);
}
