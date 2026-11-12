#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/InventoryExports.hpp"
#include "prx/libSceAudioOut/include/LaterExports.hpp"
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestCustomHrtfExports() {
    bool threw = false;
    try {
        sceAudioOut2CustomHrtfIsExist();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutCustomHrtfIsExist();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutAssignHRTF();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOut2GetHrtfIdForCronos();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOut2SetHrtfIdForCronos();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

void TestUserManagementAndPointerValidation() {
    constexpr int errorInvalidArgument = static_cast<int>(0x80260502);
    constexpr int errorInvalidParam = static_cast<int>(0x80268001);
    constexpr int errorInvalidUserHandle = static_cast<int>(0x80268010);

    Require(sceAudioOut2UserCreate(0x10000000, nullptr) == errorInvalidArgument);
    Require(sceAudioOut2UserCreate(0, nullptr) == errorInvalidArgument);
    Require(sceAudioOut2UserCreate(0xFF, nullptr) == errorInvalidArgument);
    Require(sceAudioOut2UserCreateEx(0x10000000, nullptr, nullptr) == errorInvalidArgument);

    AudioOut2UserHandle userHandle = 0;
    Require(sceAudioOut2UserCreate(0x10000000, &userHandle) == 0);
    Require(userHandle > 0);

    Require(sceAudioOut2UserGetSupportedAttributes(0, nullptr, nullptr) == errorInvalidUserHandle);
    Require(sceAudioOut2UserGetSupportedAttributes(~0u, nullptr, nullptr) == errorInvalidUserHandle);

    uint32_t ctxAttr = 0;
    uint32_t portAttr = 0;
    Require(sceAudioOut2UserGetSupportedAttributes(0, &ctxAttr, &portAttr) == errorInvalidUserHandle);
    Require(sceAudioOut2UserGetSupportedAttributes(~0u, &ctxAttr, &portAttr) == errorInvalidUserHandle);

    Require(sceAudioOut2UserGetSupportedAttributes(userHandle, nullptr, nullptr) == errorInvalidParam);

    ctxAttr = 0xFFFFFFFFu;
    portAttr = 0;
    Require(sceAudioOut2UserGetSupportedAttributes(userHandle, &ctxAttr, &portAttr) == 0);
    Require(ctxAttr == 0);
    Require(portAttr == ((1u << 0) | (1u << 1)));

    ctxAttr = 0xFFFFFFFFu;
    Require(sceAudioOut2UserGetSupportedAttributes(userHandle, &ctxAttr, nullptr) == 0);
    Require(ctxAttr == 0);

    portAttr = 0;
    Require(sceAudioOut2UserGetSupportedAttributes(userHandle, nullptr, &portAttr) == 0);
    Require(portAttr == ((1u << 0) | (1u << 1)));

    Require(sceAudioOut2UserDestroy(userHandle) == 0);

    bool threw = false;
    try {
        sceAudioOut2UserGetSupportedAttributes(userHandle, &ctxAttr, &portAttr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

void Test3DLatencyUserIdValidation() {
    constexpr int errorInvalidParam = static_cast<int>(0x80268001);

    Require(sceAudioOut2Set3DLatency(0, 0) == 0);
    Require(sceAudioOut2Set3DLatency(0, 1) == 0);
    Require(sceAudioOut2Set3DLatency(0, 2) == 0);
    Require(sceAudioOut2Set3DLatency(0, 3) == errorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0, 100) == errorInvalidParam);

    Require(sceAudioOut2Set3DLatency(0xFF, 1) == 0);
    Require(sceAudioOut2Set3DLatency(0xFF, 2) == 0);
    Require(sceAudioOut2Set3DLatency(0xFF, 0) == errorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0xFF, 3) == errorInvalidParam);

    const int invalidUserIds[] = {
        1,
        2,
        -1,
        0x10000000,
        0x10000001,
        0x20000000,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()
    };

    for (int uid : invalidUserIds) {
        Require(sceAudioOut2Set3DLatency(uid, 0) == errorInvalidParam);
        Require(sceAudioOut2Set3DLatency(uid, 1) == errorInvalidParam);
        Require(sceAudioOut2Set3DLatency(uid, 2) == errorInvalidParam);
    }
}

void TestSystemStatePointerValidation() {
    constexpr int errorInvalidArgument = static_cast<int>(0x80260502);

    Require(sceAudioOut2GetSystemState(nullptr) == errorInvalidArgument);

    AudioOut2SystemState state{};
    state.loudness = 1.0f;
    Require(sceAudioOut2GetSystemState(&state) == 0);
    Require(state.loudness == 0.0f);
}

void TestAudioOutPortAndPointerBounds() {
    constexpr int invalidPointer = static_cast<int>(0x80260004);
    constexpr int invalidHandle = static_cast<int>(0x80260001);
    constexpr int invalidPort = -2144993277;

    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));

    Require(sceAudioOutGetPortState(invalidHandle, nullptr) == invalidPointer);
    Require(sceAudioOutGetPortState(0, nullptr) == invalidPointer);
    Require(sceAudioOutGetPortState(-1, nullptr) == invalidPointer);

    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(0, &state) == invalidPort);
    Require(sceAudioOutGetPortState(-1, &state) == invalidPort);

    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);

    Require(sceAudioOutGetPortState(port, &state) == 0);
    Require(sceAudioOutGetPortState(port, nullptr) == invalidPointer);

    Require(sceAudioOutClose(port) == 0);
    Require(sceAudioOutClose(port) == invalidPort);
}

} // namespace

int main() {
    TestCustomHrtfExports();
    TestUserManagementAndPointerValidation();
    Test3DLatencyUserIdValidation();
    TestSystemStatePointerValidation();
    TestAudioOutPortAndPointerBounds();

    sceAudioOut2Terminate();
    LibcRunShutdown_nid_postfix();
    return 0;
}
