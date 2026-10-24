#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <span>

extern "C" {
int APS5_VABI sceSharePlayInitialize(void*, std::size_t);
int APS5_VABI sceSharePlayTerminate(void);
int APS5_VABI sceSharePlayGetCurrentConnectionInfoA(SharePlayConnectionInfoA*);
int APS5_VABI sceSharePlayProhibitController(std::uint8_t);
}

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

int main() {
    alignas(16) static unsigned char heap[0x1800];
    constexpr int notInitialized = static_cast<int>(0x810E0004);
    constexpr int alreadyInitialized = static_cast<int>(0x810E0003);
    constexpr int invalidArgument = static_cast<int>(0x810E0001);
    Require(sceSharePlayTerminate() == notInitialized);
    Require(sceSharePlayGetCurrentConnectionInfoA(nullptr) == notInitialized);
    Require(sceSharePlayProhibitController(2) == notInitialized);
    Require(sceSharePlayInitialize(nullptr, sizeof(heap)) == 0);
    Require(sceSharePlayInitialize(heap, 0) == alreadyInitialized);
    Require(sceSharePlayTerminate() == 0);
    Require(sceSharePlayInitialize(heap, sizeof(heap)) == 0);
    Require(sceSharePlayTerminate() == 0);
    for (const std::size_t size : {std::size_t{0}, std::size_t{1}, sizeof(heap) - 1}) Require(sceSharePlayInitialize(heap, size) == invalidArgument);
    Require(sceSharePlayInitialize(nullptr, 0) == 0);
    SharePlayConnectionInfoA info;
    std::memset(&info, 0x5a, sizeof(info));
    Require(sceSharePlayGetCurrentConnectionInfoA(&info) == 0);
    Require(info.status == 0 && info.mode == 0 && info.hostAccountId == 0 && info.visitorAccountId == 0);
    Require(info.hostOnlineId.data[0] == 0 && info.visitorOnlineId.data[0] == 0 && info.hostOnlineId.pad[2] == 0);
    Require(info.hostUserId == -1 && info.visitorUserId == -1);
    Require(sceSharePlayGetCurrentConnectionInfoA(nullptr) == static_cast<int>(0x810E0001));
    Require(sceSharePlayProhibitController(1) == 0);
    Require(sceSharePlayProhibitController(1) == 0);
    Require(sceSharePlayProhibitController(0) == 0);
    Require(sceSharePlayProhibitController(2) == invalidArgument);
    Require(sceSharePlayProhibitController(255) == invalidArgument);
    Require(sceSharePlayTerminate() == 0);
    std::memset(&info, 0x5a, sizeof(info));
    Require(sceSharePlayGetCurrentConnectionInfoA(&info) == notInitialized);
    for (const auto value : std::as_bytes(std::span(&info, 1))) Require(value == std::byte{0x5a});
    Require(sceSharePlayTerminate() == notInitialized);
}
