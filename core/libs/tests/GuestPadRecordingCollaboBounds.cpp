#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

constexpr std::uint32_t PadHistoryBufferSize = 0x960;
constexpr int ErrorNotInitialized = static_cast<int>(0x80920005u);

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestPreInitializationBounds() {
    Require(Throws([] { scePadStartRecording(); }));
    Require(Throws([] { scePadStopRecording(); }));
    Require(Throws([] { scePadRegisterCollaboInputMgrProcess(); }));

    for (int handle : {-500, -1, 0, 1, 2, 500}) {
        for (std::uint32_t size : {0u, 1u, 100u, 0x95fu, PadHistoryBufferSize, 0x961u, 4096u}) {
            Require(scePadReadHistory(handle, nullptr, size) == ErrorNotInitialized);
        }
    }
}

void TestPostInitializationBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadInit_nid_postfix() == PAD_OK);

    Require(Throws([] { scePadStartRecording(); }));
    Require(Throws([] { scePadStopRecording(); }));
    Require(Throws([] { scePadRegisterCollaboInputMgrProcess(); }));

    for (int handle : {-500, -1, 0, 1, 2, 3, 4, 16, 32, 47, 500}) {
        for (std::uint32_t invalidSize : {0u, 1u, 100u, 0x95fu, 0x961u, 0xffffffffu}) {
            Require(scePadReadHistory(handle, nullptr, invalidSize) == PAD_ERROR_INVALID_ARG);
        }
        Require(Throws([&] { scePadReadHistory(handle, nullptr, PadHistoryBufferSize); }));
    }
}

void TestOpenStandardHandleBounds() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 > 0);

    for (int i = 0; i < 5; ++i) {
        Require(Throws([] { scePadStartRecording(); }));
        Require(Throws([] { scePadStopRecording(); }));
        Require(Throws([] { scePadRegisterCollaboInputMgrProcess(); }));
    }

    alignas(8) std::uint8_t historyBuffer[PadHistoryBufferSize]{};
    PadHistoryData* historyPtr = reinterpret_cast<PadHistoryData*>(historyBuffer);

    Require(Throws([&] { scePadReadHistory(handle0, historyPtr, PadHistoryBufferSize); }));
    Require(scePadReadHistory(handle0, historyPtr, 0) == PAD_ERROR_INVALID_ARG);

    Require(scePadClose_nid_postfix(handle0) == PAD_OK);

    Require(scePadReadHistory(handle0, historyPtr, 0) == PAD_ERROR_INVALID_ARG);
    Require(Throws([&] { scePadReadHistory(handle0, historyPtr, PadHistoryBufferSize); }));
}

void TestSpecialAndRemoteHandles() {
    const int specialHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle >= PAD_SPECIAL_HANDLE);

    Require(Throws([] { scePadStartRecording(); }));
    Require(Throws([] { scePadStopRecording(); }));
    Require(Throws([] { scePadRegisterCollaboInputMgrProcess(); }));

    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK);

    const int remoteHandle = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remoteHandle == PAD_REMOTE_HANDLE);

    Require(Throws([] { scePadStartRecording(); }));
    Require(Throws([] { scePadStopRecording(); }));
    Require(Throws([] { scePadRegisterCollaboInputMgrProcess(); }));

    Require(scePadClose_nid_postfix(remoteHandle) == PAD_OK);
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationBounds();
    TestOpenStandardHandleBounds();
    TestSpecialAndRemoteHandles();
    return 0;
}
