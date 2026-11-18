#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestUninitializedAudioOut2() {
    constexpr int ErrorInvalidArgument = static_cast<int>(0x80260502);
    constexpr int ErrorNotInitialized = static_cast<int>(0x80268006);

    AudioOut2SystemState sysState{};
    Require(sceAudioOut2GetSystemState(nullptr) == ErrorInvalidArgument);
    sysState.loudness = 1.0f;
    Require(sceAudioOut2GetSystemState(&sysState) == 0);
    Require(sysState.loudness == 0.0f);

    Require(sceAudioOut2EnableChat() == ErrorNotInitialized);
    Require(sceAudioOut2DisableChat() == ErrorNotInitialized);
    Require(sceAudioOut2SetSystemDebugState(nullptr) == ErrorNotInitialized);

    AudioOut2SystemDebugStateParam dbg{};
    dbg.debug_state_id = 1;
    dbg.param_size = 8;
    std::uint64_t dummyPayload = 12345;
    dbg.param = &dummyPayload;
    Require(sceAudioOut2SetSystemDebugState(&dbg) == ErrorNotInitialized);
    Require(sceAudioOut2GetSystemStateEx(nullptr) == ErrorNotInitialized);

    std::uint8_t stateExBuf[64] = {};
    Require(sceAudioOut2GetSystemStateEx(stateExBuf) == ErrorNotInitialized);
}

void TestInitializationAndChat() {
    constexpr int ErrorAlreadyInitialized = static_cast<int>(0x80268004);

    Require(sceAudioOut2Initialize() == 0);
    Require(sceAudioOut2Initialize() == ErrorAlreadyInitialized);
    Require(sceAudioOut2InitializeEx(0) == ErrorAlreadyInitialized);
    Require(sceAudioOut2InitializeEx(1) == ErrorAlreadyInitialized);

    Require(sceAudioOut2EnableChat() == 0);
    Require(sceAudioOut2EnableChat() == 0);
    Require(sceAudioOut2DisableChat() == 0);
    Require(sceAudioOut2DisableChat() == 0);
}

void TestSystemDebugStateAndGates() {
    constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);
    constexpr int ErrorInvalidPointer = static_cast<int>(0x8026800C);
    constexpr int ErrorDebugStateTwo = static_cast<int>(0x80268000);

    Require(sceAudioOut2SetSystemDebugState(nullptr) == ErrorInvalidPointer);

    std::uint64_t dummyPayload = 12345;
    AudioOut2SystemDebugStateParam dbg{};

    dbg.debug_state_id = 0;
    dbg.param_size = 8;
    dbg.param = &dummyPayload;
    Require(sceAudioOut2SetSystemDebugState(&dbg) == ErrorInvalidParam);

    dbg.debug_state_id = 2;
    dbg.param_size = 8;
    dbg.param = &dummyPayload;
    Require(sceAudioOut2SetSystemDebugState(&dbg) == ErrorDebugStateTwo);

    dbg.debug_state_id = 1;
    dbg.param_size = 4;
    dbg.param = &dummyPayload;
    Require(sceAudioOut2SetSystemDebugState(&dbg) == ErrorInvalidParam);

    dbg.debug_state_id = 1;
    dbg.param_size = 16;
    dbg.param = &dummyPayload;
    Require(sceAudioOut2SetSystemDebugState(&dbg) == ErrorInvalidParam);

    dbg.debug_state_id = 0x20;
    dbg.param_size = 4;
    dbg.param = &dummyPayload;
    bool threwUnmodelledDebugState = false;
    try {
        sceAudioOut2SetSystemDebugState(&dbg);
    } catch (const std::runtime_error&) {
        threwUnmodelledDebugState = true;
    }
    Require(threwUnmodelledDebugState);

    dbg.debug_state_id = 1;
    dbg.param_size = 8;
    dbg.param = nullptr;
    Require(sceAudioOut2SetSystemDebugState(&dbg) == ErrorInvalidPointer);

    dbg.debug_state_id = 1;
    dbg.param_size = 8;
    dbg.param = &dummyPayload;
    Require(sceAudioOut2SetSystemDebugState(&dbg) == 0);

    bool threwStateEx = false;
    try {
        sceAudioOut2GetSystemStateEx(nullptr);
    } catch (const std::runtime_error&) {
        threwStateEx = true;
    }
    Require(threwStateEx);
}

void Test3DLatencyBounds() {
    constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);

    Require(sceAudioOut2Set3DLatency(0, 3) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0, 100) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0, 0) == 0);
    Require(sceAudioOut2Set3DLatency(0, 1) == 0);
    Require(sceAudioOut2Set3DLatency(0, 2) == 0);
    Require(sceAudioOut2Set3DLatency(0xFF, 0) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0xFF, 1) == 0);
    Require(sceAudioOut2Set3DLatency(0xFF, 2) == 0);
    Require(sceAudioOut2Set3DLatency(1, 0) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(1, 1) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(1, 2) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(-1, 1) == ErrorInvalidParam);
}

void TestUserLifecycleAndAttributes() {
    constexpr int ErrorInvalidArgument = static_cast<int>(0x80260502);
    constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);
    constexpr int ErrorInvalidUserHandle = static_cast<int>(0x80268010);

    Require(sceAudioOut2UserCreate(0, nullptr) == ErrorInvalidArgument);
    Require(sceAudioOut2UserCreate(0x10000000, nullptr) == ErrorInvalidArgument);
    Require(sceAudioOut2UserCreateEx(0, nullptr, nullptr) == ErrorInvalidArgument);

    AudioOut2UserHandle u1 = 0;
    AudioOut2UserHandle u2 = 0;
    AudioOut2UserHandle u3 = 0;
    Require(sceAudioOut2UserCreate(0x10000000, &u1) == 0);
    Require(u1 != 0);
    Require(sceAudioOut2UserCreateEx(0x10000001, nullptr, &u2) == 0);
    Require(u2 != 0 && u2 != u1);
    const std::uint32_t dummyAttr = 123;
    Require(sceAudioOut2UserCreateEx(0x10000002, &dummyAttr, &u3) == 0);
    Require(u3 != 0 && u3 != u1 && u3 != u2);

    for (const AudioOut2UserHandle bHandle : {AudioOut2UserHandle{0}, std::numeric_limits<AudioOut2UserHandle>::max()}) {
        std::uint32_t cAttr = 0xAA;
        std::uint32_t pAttr = 0xBB;
        Require(sceAudioOut2UserGetSupportedAttributes(bHandle, &cAttr, &pAttr) == ErrorInvalidUserHandle);
        Require(sceAudioOut2UserGetSupportedAttributes(bHandle, nullptr, &pAttr) == ErrorInvalidUserHandle);
        Require(sceAudioOut2UserGetSupportedAttributes(bHandle, &cAttr, nullptr) == ErrorInvalidUserHandle);
        Require(sceAudioOut2UserGetSupportedAttributes(bHandle, nullptr, nullptr) == ErrorInvalidUserHandle);
        Require(cAttr == 0xAA && pAttr == 0xBB);
    }

    Require(sceAudioOut2UserGetSupportedAttributes(u1, nullptr, nullptr) == ErrorInvalidParam);

    std::uint32_t ctxAttr = 0xFF;
    std::uint32_t portAttr = 0xFF;
    Require(sceAudioOut2UserGetSupportedAttributes(u1, &ctxAttr, &portAttr) == 0);
    Require(ctxAttr == 0);
    Require(portAttr == 3);

    portAttr = 0xFF;
    Require(sceAudioOut2UserGetSupportedAttributes(u2, nullptr, &portAttr) == 0);
    Require(portAttr == 3);

    ctxAttr = 0xFF;
    Require(sceAudioOut2UserGetSupportedAttributes(u3, &ctxAttr, nullptr) == 0);
    Require(ctxAttr == 0);

    Require(sceAudioOut2UserDestroy(u1) == 0);
    Require(sceAudioOut2UserDestroy(u1) == 0);

    bool threwDestroyed = false;
    try {
        sceAudioOut2UserGetSupportedAttributes(u1, &ctxAttr, &portAttr);
    } catch (const std::runtime_error&) {
        threwDestroyed = true;
    }
    Require(threwDestroyed);

    bool threwUnregistered = false;
    try {
        sceAudioOut2UserGetSupportedAttributes(0x76543210, &ctxAttr, &portAttr);
    } catch (const std::runtime_error&) {
        threwUnregistered = true;
    }
    Require(threwUnregistered);

    Require(sceAudioOut2UserDestroy(u2) == 0);
    Require(sceAudioOut2UserDestroy(u3) == 0);
}

} // namespace

int main() {
    TestUninitializedAudioOut2();
    TestInitializationAndChat();
    TestSystemDebugStateAndGates();
    Test3DLatencyBounds();
    TestUserLifecycleAndAttributes();
    Require(sceAudioOut2Terminate() == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
