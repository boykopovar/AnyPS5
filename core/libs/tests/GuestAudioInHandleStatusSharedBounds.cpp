#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "prx/libSceAudioIn/include/Exports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int audioInErrorInvalidHandle = static_cast<int>(0x80260101);
constexpr int audioInErrorInvalidState = static_cast<int>(0x80260109);
constexpr std::uint32_t handleBase = 0x30000000;
constexpr int userId = 0x10000000;

struct AudioInHandleStatusLayout {
    std::uint8_t entryBlob[16];
    std::int32_t mode;
    std::uint32_t shmField402c;
    std::uint32_t shmField4028;
};

static_assert(sizeof(AudioInHandleStatusLayout) == 28);
static_assert(offsetof(AudioInHandleStatusLayout, mode) == 0x10);
static_assert(offsetof(AudioInHandleStatusLayout, shmField402c) == 0x14);
static_assert(offsetof(AudioInHandleStatusLayout, shmField4028) == 0x18);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TAction>
bool Throws(TAction&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestGetHandleStatusInfoHandleGates() {
    Require(sceAudioInInit() == 0);

    AudioInHandleStatusLayout layout{};
    std::array<std::uint8_t, 64> buffer{};

    for (const int badHandle : {-100, -1, 0, 1, 2, 7, 8, 100, 255}) {
        Require(sceAudioInGetHandleStatusInfo(badHandle, nullptr) == audioInErrorInvalidHandle);
        Require(sceAudioInGetHandleStatusInfo(badHandle, &layout) == audioInErrorInvalidHandle);
        Require(sceAudioInGetHandleStatusInfo(badHandle, buffer.data()) == audioInErrorInvalidHandle);
    }

    for (const int badBase : {0x10000000, 0x20000000, 0x40000000, 0x70000000, 0x00040000, 0x00040001, 0x00040007, 0x31000000, 0x30010000}) {
        Require(sceAudioInGetHandleStatusInfo(badBase, nullptr) == audioInErrorInvalidHandle);
        Require(sceAudioInGetHandleStatusInfo(badBase, &layout) == audioInErrorInvalidHandle);
    }

    for (const std::uint32_t badIndex : {8u, 9u, 10u, 16u, 32u, 64u, 128u, 255u}) {
        const int badIndexHandle = static_cast<int>(handleBase | badIndex);
        Require(sceAudioInGetHandleStatusInfo(badIndexHandle, nullptr) == audioInErrorInvalidHandle);
        Require(sceAudioInGetHandleStatusInfo(badIndexHandle, &layout) == audioInErrorInvalidHandle);
    }

    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        const int unopenedHandle = static_cast<int>(handleBase | slot);
        Require(sceAudioInGetHandleStatusInfo(unopenedHandle, nullptr) == audioInErrorInvalidState);
        Require(sceAudioInGetHandleStatusInfo(unopenedHandle, &layout) == audioInErrorInvalidState);
        Require(sceAudioInGetHandleStatusInfo(unopenedHandle, buffer.data()) == audioInErrorInvalidState);
    }
}

void TestIsSharedDeviceIndexGates() {
    for (const int badIndex : {8, 9, 10, 15, 20, 50, 100, 255, -1, -2, -100}) {
        Require(sceAudioInIsSharedDevice(badIndex) == audioInErrorInvalidHandle);
    }

    for (const std::uint32_t badIndex : {8u, 9u, 10u, 15u, 20u, 255u}) {
        const int badHandle = static_cast<int>(handleBase | badIndex);
        Require(sceAudioInIsSharedDevice(badHandle) == audioInErrorInvalidHandle);
    }

    for (const int vmicBad : {0x00040008, 0x00040009, 0x000400ff}) {
        Require(sceAudioInIsSharedDevice(vmicBad) == audioInErrorInvalidHandle);
    }

    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        Require(sceAudioInIsSharedDevice(static_cast<int>(slot)) == audioInErrorInvalidState);
        Require(sceAudioInIsSharedDevice(static_cast<int>(handleBase | slot)) == audioInErrorInvalidState);
        Require(sceAudioInIsSharedDevice(static_cast<int>(0x00040000 | slot)) == audioInErrorInvalidState);
        Require(sceAudioInIsSharedDevice(static_cast<int>(0x12340000 | slot)) == audioInErrorInvalidState);
    }
}

void TestStatusTransitionsAndSlotLifecycle() {
    AudioInHandleStatusLayout layout{};
    std::array<int, 8> handles{};

    Require(Throws([&] { sceAudioInSetMode(0, 0x42); }));
    Require(Throws([&] { sceAudioInSetMode2(0, 0x42, nullptr); }));
    Require(Throws([&] { sceAudioInSetMode(1, 0x10); }));
    Require(Throws([&] { sceAudioInSetMode2(1, 0x10, nullptr); }));

    for (int i = 0; i < 8; ++i) {
        handles[i] = sceAudioInOpen(userId, 0, 0, 128, 48000, 2);
        Require(handles[i] >= static_cast<int>(handleBase));

        const std::uint32_t slot = static_cast<std::uint32_t>(handles[i]) & 0xffu;
        Require(slot < 8);

        Require(Throws([&] { sceAudioInGetHandleStatusInfo(handles[i], nullptr); }));
        Require(Throws([&] { sceAudioInGetHandleStatusInfo(handles[i], &layout); }));

        Require(Throws([&] { sceAudioInIsSharedDevice(handles[i]); }));
        Require(Throws([&] { sceAudioInIsSharedDevice(static_cast<int>(slot)); }));
        Require(Throws([&] { sceAudioInIsSharedDevice(static_cast<int>(0x12340000 | slot)); }));
    }

    for (int i = 0; i < 8; ++i) {
        const int h = handles[i];
        const std::uint32_t slot = static_cast<std::uint32_t>(h) & 0xffu;

        Require(sceAudioInClose(h) == 0);
        Require(sceAudioInClose(h) == audioInErrorInvalidState);

        Require(sceAudioInGetHandleStatusInfo(h, nullptr) == audioInErrorInvalidState);
        Require(sceAudioInGetHandleStatusInfo(h, &layout) == audioInErrorInvalidState);

        Require(sceAudioInIsSharedDevice(h) == audioInErrorInvalidState);
        Require(sceAudioInIsSharedDevice(static_cast<int>(slot)) == audioInErrorInvalidState);
        Require(sceAudioInIsSharedDevice(static_cast<int>(0x12340000 | slot)) == audioInErrorInvalidState);
    }

    const int reused = sceAudioInOpen(userId, 0, 0, 128, 48000, 2);
    Require(reused >= static_cast<int>(handleBase));
    Require(Throws([&] { sceAudioInGetHandleStatusInfo(reused, &layout); }));
    Require(Throws([&] { sceAudioInIsSharedDevice(reused); }));
    Require(sceAudioInClose(reused) == 0);
    Require(sceAudioInGetHandleStatusInfo(reused, &layout) == audioInErrorInvalidState);
    Require(sceAudioInIsSharedDevice(reused) == audioInErrorInvalidState);
}

} // namespace

int main() {
    TestGetHandleStatusInfoHandleGates();
    TestIsSharedDeviceIndexGates();
    TestStatusTransitionsAndSlotLifecycle();

    LibcRunShutdown_nid_postfix();
    return 0;
}
