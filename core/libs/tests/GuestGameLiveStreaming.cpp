#include "prx/libSceGameLiveStreaming/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
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

constexpr int kInvalidArgument = static_cast<int>(0x80A00002);

GameLiveStreamingFeedbackMessage* AsMessages(std::array<std::uint8_t, 256>& buffer) {
    return reinterpret_cast<GameLiveStreamingFeedbackMessage*>(buffer.data());
}

void CheckLifecycleFirst() {
    constexpr int notInitialized = static_cast<int>(0x80A00004);
    GameLiveStreamingSpoilerTag tag{};
    GameLiveStreamingPresetCommand preset{};
    GameLiveStreamingCameraFrameSetting camera{};
    std::array<std::uint8_t, 256> messages{};
    std::uint32_t count = 0x55;
    std::size_t acquired = 0x55;
    Require(sceGameLiveStreamingPermitLiveStreaming(true) == notInitialized);
    Require(sceGameLiveStreamingPermitServerSideRecording(false) == notInitialized);
    Require(sceGameLiveStreamingSetMaxBitrate(6000) == notInitialized);
    Require(sceGameLiveStreamingEnableSocialFeedback(true) == notInitialized);
    Require(sceGameLiveStreamingSetMetadata(nullptr) == notInitialized);
    Require(sceGameLiveStreamingSetMetadata("metadata") == notInitialized);
    Require(sceGameLiveStreamingSetSpoilerTag(0, nullptr) == notInitialized);
    Require(sceGameLiveStreamingSetSpoilerTag(1, &tag) == notInitialized);
    Require(sceGameLiveStreamingClearSpoilerTag() == notInitialized);
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(0, nullptr) == notInitialized);
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(1, &preset) == notInitialized);
    Require(sceGameLiveStreamingClearPresetSocialFeedbackCommands() == notInitialized);
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription(nullptr) == notInitialized);
    Require(sceGameLiveStreamingSetCameraFrameSetting(nullptr) == notInitialized);
    Require(sceGameLiveStreamingSetCameraFrameSetting(&camera) == notInitialized);
    Require(sceGameLiveStreamingSetInvitationSessionId(nullptr) == notInitialized);
    Require(sceGameLiveStreamingGetSocialFeedbackMessagesCount(0, nullptr) == notInitialized);
    Require(sceGameLiveStreamingGetSocialFeedbackMessagesCount(1, &count) == notInitialized);
    Require(count == 0x55);
    Require(sceGameLiveStreamingGetSocialFeedbackMessages(0, 1, nullptr, nullptr) == notInitialized);
    Require(sceGameLiveStreamingGetSocialFeedbackMessages(1, 1, AsMessages(messages), &acquired) == notInitialized);
    Require(acquired == 0x55);
    Require(sceGameLiveStreamingClearSocialFeedbackMessages(0) == notInitialized);
    Require(sceGameLiveStreamingClearSocialFeedbackMessages(1) == notInitialized);
    Require(sceGameLiveStreamingPostSocialMessage(nullptr) == notInitialized);
    Require(sceGameLiveStreamingPostSocialMessage("message") == notInitialized);
}

void CheckString(int (APS5_VABI *call)(const char*), std::size_t limit) {
    Require(call(nullptr) == kInvalidArgument);
    Require(call("") == 0);
    Require(call(std::string(limit, 'a').c_str()) == 0);
    Require(call(std::string(limit + 1, 'a').c_str()) == kInvalidArgument);
    Require(call(std::string(limit + 100, 'a').c_str()) == kInvalidArgument);
}

void CheckStrings() {
    CheckString(sceGameLiveStreamingSetMetadata, 240);
    CheckString(sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription, 720);
    CheckString(sceGameLiveStreamingSetInvitationSessionId, 45);
    CheckString(sceGameLiveStreamingPostSocialMessage, 360);
}

void CheckFeedbackTypes() {
    std::array<std::uint8_t, 256> messages{};
    for (const int type : {-1, 0, 4, 5, 0x7fffffff}) {
        std::uint32_t count = 0xdeadbeef;
        Require(sceGameLiveStreamingGetSocialFeedbackMessagesCount(type, &count) == kInvalidArgument);
        Require(count == 0xdeadbeef);
        std::size_t acquired = 0xdeadbeef;
        Require(sceGameLiveStreamingGetSocialFeedbackMessages(type, 1, AsMessages(messages), &acquired) == kInvalidArgument);
        Require(acquired == 0xdeadbeef);
        Require(sceGameLiveStreamingClearSocialFeedbackMessages(type) == kInvalidArgument);
    }
    for (const int type : {1, 2, 3}) {
        std::uint32_t count = 0xdeadbeef;
        Require(sceGameLiveStreamingGetSocialFeedbackMessagesCount(type, &count) == 0);
        Require(count == 0);
        messages.fill(0xaa);
        std::size_t acquired = 0xdeadbeef;
        Require(sceGameLiveStreamingGetSocialFeedbackMessages(type, 1, AsMessages(messages), &acquired) == 0);
        Require(acquired == 0);
        for (const std::uint8_t byte : messages) Require(byte == 0xaa);
        acquired = 0xdeadbeef;
        Require(sceGameLiveStreamingGetSocialFeedbackMessages(type, 0, AsMessages(messages), &acquired) == 0);
        Require(acquired == 0);
        Require(sceGameLiveStreamingClearSocialFeedbackMessages(type) == 0);
    }
    std::size_t acquired = 0xdeadbeef;
    Require(sceGameLiveStreamingGetSocialFeedbackMessages(1, 0, nullptr, &acquired) == kInvalidArgument);
    Require(sceGameLiveStreamingGetSocialFeedbackMessages(1, 1, nullptr, &acquired) == kInvalidArgument);
    Require(sceGameLiveStreamingGetSocialFeedbackMessages(1, 1, AsMessages(messages), nullptr) == kInvalidArgument);
    Require(acquired == 0xdeadbeef);
    Require(sceGameLiveStreamingGetSocialFeedbackMessagesCount(1, nullptr) == kInvalidArgument);
}

void CheckArrays() {
    GameLiveStreamingSpoilerTag tags[17]{};
    Require(sceGameLiveStreamingSetSpoilerTag(0, nullptr) == kInvalidArgument);
    Require(sceGameLiveStreamingSetSpoilerTag(1, nullptr) == kInvalidArgument);
    Require(sceGameLiveStreamingSetSpoilerTag(0, tags) == 0);
    Require(sceGameLiveStreamingSetSpoilerTag(1, tags) == 0);
    Require(sceGameLiveStreamingSetSpoilerTag(16, tags) == 0);
    Require(sceGameLiveStreamingSetSpoilerTag(17, tags) == kInvalidArgument);
    Require(sceGameLiveStreamingSetSpoilerTag(0x1000, tags) == kInvalidArgument);
    for (const std::size_t index : {0u, 15u}) {
        for (const std::size_t byte : {0u, 15u, 30u}) {
            tags[index].reserved[byte] = 1;
            Require(sceGameLiveStreamingSetSpoilerTag(16, tags) == kInvalidArgument);
            Require(sceGameLiveStreamingSetSpoilerTag(index, tags) == 0);
            tags[index].reserved[byte] = 0;
        }
    }
    tags[16].reserved[0] = 1;
    Require(sceGameLiveStreamingSetSpoilerTag(16, tags) == 0);
    tags[0].text[192] = 'x';
    Require(sceGameLiveStreamingSetSpoilerTag(1, tags) == 0);
    Require(sceGameLiveStreamingClearSpoilerTag() == 0);
    Require(sceGameLiveStreamingClearSpoilerTag() == 0);

    GameLiveStreamingPresetCommand presets[6]{};
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(0, nullptr) == kInvalidArgument);
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(1, nullptr) == kInvalidArgument);
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(0, presets) == 0);
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(5, presets) == 0);
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(6, presets) == kInvalidArgument);
    for (const std::size_t index : {0u, 4u}) {
        for (const std::size_t byte : {0u, 30u}) {
            presets[index].reserved[byte] = 1;
            Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(5, presets) == kInvalidArgument);
            Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(index, presets) == 0);
            presets[index].reserved[byte] = 0;
        }
    }
    presets[5].reserved[0] = 1;
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(5, presets) == 0);
    presets[0].body[196] = 1;
    Require(sceGameLiveStreamingSetPresetSocialFeedbackCommands(5, presets) == 0);
    Require(sceGameLiveStreamingClearPresetSocialFeedbackCommands() == 0);
    Require(sceGameLiveStreamingClearPresetSocialFeedbackCommands() == 0);

    GameLiveStreamingCameraFrameSetting camera{};
    Require(sceGameLiveStreamingSetCameraFrameSetting(nullptr) == kInvalidArgument);
    Require(sceGameLiveStreamingSetCameraFrameSetting(&camera) == 0);
    camera.head[8] = 1;
    Require(sceGameLiveStreamingSetCameraFrameSetting(&camera) == 0);
    camera.head[8] = 0;
    for (const std::size_t byte : {0u, 15u, 30u}) {
        camera.reserved[byte] = 1;
        Require(sceGameLiveStreamingSetCameraFrameSetting(&camera) == kInvalidArgument);
        camera.reserved[byte] = 0;
    }
}

void CheckScalars() {
    for (const bool value : {true, false}) {
        Require(sceGameLiveStreamingPermitLiveStreaming(value) == 0);
        Require(sceGameLiveStreamingPermitServerSideRecording(value) == 0);
        Require(sceGameLiveStreamingEnableSocialFeedback(value) == 0);
    }
    for (const std::uint32_t bitrate : {0u, 6000u, 0xffffffffu}) Require(sceGameLiveStreamingSetMaxBitrate(bitrate) == 0);
}

}

int main() {
    constexpr int invalidArgument = static_cast<int>(0x80A00002);
    constexpr int alreadyInitialized = static_cast<int>(0x80A00003);
    constexpr int notInitialized = static_cast<int>(0x80A00004);
    Require(sceGameLiveStreamingTerminate() == notInitialized);
    Require(sceGameLiveStreamingGetCurrentStatus2(nullptr) == notInitialized);
    Require(EnableThrowsNotInitialized(true));
    CheckLifecycleFirst();
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

    CheckScalars();
    CheckStrings();
    CheckFeedbackTypes();
    CheckArrays();

    Require(sceGameLiveStreamingEnableLiveStreaming(false) == 0);
    Require(sceGameLiveStreamingTerminate() == 0);
    CheckLifecycleFirst();
    Require(EnableThrowsNotInitialized(false));
    std::memset(&status, 0x7f, sizeof(status));
    Require(sceGameLiveStreamingGetCurrentStatus2(&status) == notInitialized);
    for (const auto value : std::as_bytes(std::span(&status, 1))) Require(value == std::byte{0x7f});
    programInfo.fill(0x7f);
    Require(sceGameLiveStreamingGetProgramInfo(reinterpret_cast<GameLiveStreamingProgramInfo*>(programInfo.data())) == notInitialized);
    for (const std::uint8_t byte : programInfo) Require(byte == 0x7f);
    Require(sceGameLiveStreamingTerminate() == notInitialized);
}
