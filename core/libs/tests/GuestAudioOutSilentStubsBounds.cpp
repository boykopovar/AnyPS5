#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

constexpr int audioOutErrorInvalidPort = static_cast<int>(0x80260003);
constexpr int audioOutErrorInvalidPointer = static_cast<int>(0x80260004);
constexpr int audioOut2ErrorInvalidArgument = static_cast<int>(0x80260502);
constexpr int audioOut2ErrorInvalidHandle = static_cast<int>(0x80260503);

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

void TestSilentStubsReturns() {
    Require(sceAudioOutSetMainOutput() == 0);
    Require(sceAudioOutSetMainOutput() == 0);
    Require(sceAudioOutSetMainOutput() == 0);

    Require(sceAudioOutMasteringTerm() == 0);
    Require(sceAudioOutMasteringTerm() == 0);

    Require(sceAudioOutMasteringInit(0) == 0);
    Require(sceAudioOutMasteringTerm() == 0);
    Require(Throws([&] { sceAudioOutMasteringInit(1); }));
    Require(Throws([&] { sceAudioOutMasteringInit(0x80000000u); }));

    Require(Throws([&] { sceAudioOutMasteringGetState(nullptr); }));
    Require(Throws([&] { sceAudioOutMasteringSetParam(nullptr, 0); }));

    Require(sceAudioOut2LoTerminate() == 0);
    Require(sceAudioOut2LoTerminate() == 0);

    Require(sceAudioOut2LoInit() == 0);
    Require(sceAudioOut2LoTerminate() == 0);
}

void TestAudioOut2ContextSetAttributesAndLoLifecycle() {
    for (const AudioOut2ContextHandle badCtx : {0ULL, 1ULL, 999ULL, 0xFFFFFFFFFFFFFFFFULL}) {
        Require(sceAudioOut2ContextSetAttributes(badCtx, nullptr, 0) == audioOut2ErrorInvalidHandle);
        Require(sceAudioOut2LoContextSetAttributes(badCtx, nullptr, 0) == audioOut2ErrorInvalidHandle);
        Require(sceAudioOut2ContextSetAttributes(badCtx, nullptr, 1) == audioOut2ErrorInvalidHandle);
        Require(sceAudioOut2LoContextSetAttributes(badCtx, nullptr, 1) == audioOut2ErrorInvalidHandle);
    }

    AudioOut2ContextParam ctxParams{};
    Require(sceAudioOut2ContextResetParam(&ctxParams) == 0);

    size_t memSize = 0;
    Require(sceAudioOut2ContextQueryMemory(&ctxParams, &memSize) == 0);
    Require(memSize > 0);

    std::vector<std::uint8_t> ctxBuffer(memSize);
    AudioOut2ContextHandle ctx = 0;
    Require(sceAudioOut2ContextCreate(&ctxParams, ctxBuffer.data(), memSize, &ctx) == 0);
    Require(ctx != 0);

    Require(sceAudioOut2ContextSetAttributes(ctx, nullptr, 1) == audioOut2ErrorInvalidArgument);
    Require(sceAudioOut2LoContextSetAttributes(ctx, nullptr, 1) == audioOut2ErrorInvalidArgument);
    Require(sceAudioOut2ContextSetAttributes(ctx, nullptr, 5) == audioOut2ErrorInvalidArgument);
    Require(sceAudioOut2LoContextSetAttributes(ctx, nullptr, 5) == audioOut2ErrorInvalidArgument);

    Require(sceAudioOut2ContextSetAttributes(ctx, nullptr, 0) == 0);
    Require(sceAudioOut2LoContextSetAttributes(ctx, nullptr, 0) == 0);

    std::uint32_t attrVal = 1;
    AudioOut2Attribute dummyAttr{};
    dummyAttr.attribute_id = 2;
    dummyAttr.value = &attrVal;
    dummyAttr.value_size = sizeof(attrVal);

    Require(sceAudioOut2ContextSetAttributes(ctx, &dummyAttr, 1) == 0);
    Require(sceAudioOut2LoContextSetAttributes(ctx, &dummyAttr, 1) == 0);

    Require(sceAudioOut2LoTerminate() == 0);

    Require(sceAudioOut2ContextSetAttributes(ctx, &dummyAttr, 1) == 0);
    Require(sceAudioOut2LoContextSetAttributes(ctx, &dummyAttr, 1) == 0);

    Require(sceAudioOut2ContextDestroy(ctx) == 0);

    Require(sceAudioOut2ContextSetAttributes(ctx, nullptr, 0) == audioOut2ErrorInvalidHandle);
    Require(sceAudioOut2LoContextSetAttributes(ctx, nullptr, 0) == audioOut2ErrorInvalidHandle);
}

void TestUnopenedPortStateAndGates() {
    for (const int unopenedHandle : {-100, -1, 0, 1, 2, 8, 16, 32, 33, 100}) {
        Require(sceAudioOutClose(unopenedHandle) == audioOutErrorInvalidPort);
        Require(sceAudioOutOutput(unopenedHandle, nullptr) == audioOutErrorInvalidPort);

        AudioOutPortState state{};
        Require(sceAudioOutGetPortState(unopenedHandle, &state) == audioOutErrorInvalidPort);
        Require(sceAudioOutGetPortState(unopenedHandle, nullptr) == audioOutErrorInvalidPointer);

        std::uint64_t outTime = 0;
        Require(sceAudioOutGetLastOutputTime(unopenedHandle, &outTime) == audioOutErrorInvalidPort);
        Require(sceAudioOutGetLastOutputTime(unopenedHandle, nullptr) == audioOutErrorInvalidPointer);

        Require(sceAudioOutSetVolume(unopenedHandle, 0, nullptr) == audioOutErrorInvalidPointer);
    }
}

} // namespace

int main() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000E));
    Require(sceAudioOut2Initialize() == 0 || sceAudioOut2Initialize() == static_cast<int>(0x80268004));

    TestSilentStubsReturns();
    TestAudioOut2ContextSetAttributesAndLoLifecycle();
    TestUnopenedPortStateAndGates();

    LibcRunShutdown_nid_postfix();
    return 0;
}
