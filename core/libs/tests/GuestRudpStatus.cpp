#include "prx/libSceRudp/include/Api.hpp"
#include "SceTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {

}

static void Require(bool value) { if (!value) std::abort(); }

static void APS5_VABI Handler(int, int, int, void*) {}

int main() {
    constexpr int notInitialized = static_cast<int>(0x80770001);
    constexpr int alreadyInitialized = static_cast<int>(0x80770002);
    constexpr int invalidContextId = static_cast<int>(0x80770003);
    constexpr int invalidArgument = static_cast<int>(0x80770004);
    constexpr int memory = static_cast<int>(0x80770007);
    constexpr std::size_t statusSize = 0xF8;
    constexpr std::size_t currentContexts = 0x64;
    alignas(16) static unsigned char pool[0x10000];

    std::array<unsigned char, 0x200> status;
    status.fill(0x5a);
    const auto untouched = status;
    Require(sceRudpGetStatus(status.data(), statusSize) == notInitialized);
    Require(sceRudpGetStatus(nullptr, 0) == notInitialized);
    Require(sceRudpGetStatus(status.data(), statusSize + 1) == notInitialized);
    Require(status == untouched);
    Require(sceRudpEnd() == notInitialized);
    Require(sceRudpTerminate(0) == notInitialized);

    Require(sceRudpInit_nid_postfix(nullptr, sizeof(pool)) == invalidArgument);
    Require(sceRudpInit_nid_postfix(pool, 0) == invalidArgument);
    Require(sceRudpInit_nid_postfix(pool, -1) == invalidArgument);
    Require(sceRudpInit_nid_postfix(pool, 0xC57) == memory);
    for (std::size_t offset : {1u, 2u, 4u}) Require(sceRudpInit_nid_postfix(pool + offset, 0x1000) == memory);
    Require(sceRudpInit_nid_postfix(pool + 8, 0xC58) == 0);
    Require(sceRudpInit_nid_postfix(nullptr, 0) == alreadyInitialized);

    Require(sceRudpGetStatus(nullptr, statusSize) == invalidArgument);
    Require(sceRudpGetStatus(nullptr, 0) == invalidArgument);
    Require(sceRudpGetStatus(status.data(), 0) == invalidArgument);
    Require(sceRudpGetStatus(status.data(), statusSize + 1) == invalidArgument);
    Require(status == untouched);

    Require(sceRudpGetStatus(status.data(), statusSize) == 0);
    for (std::size_t i = 0; i < statusSize; ++i) Require(status[i] == 0);
    for (std::size_t i = statusSize; i < status.size(); ++i) Require(status[i] == 0x5a);

    int first = -1;
    int second = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &first) == 0);
    Require(sceRudpCreateContext(Handler, nullptr, &second) == 0);
    status.fill(0x5a);
    Require(sceRudpGetStatus(status.data(), statusSize) == 0);
    std::uint32_t contexts = 0;
    std::memcpy(&contexts, status.data() + currentContexts, sizeof(contexts));
    Require(contexts == 2);
    for (std::size_t i = 0; i < statusSize; ++i) {
        if (i < currentContexts || i >= currentContexts + sizeof(contexts)) Require(status[i] == 0);
    }

    status.fill(0x5a);
    Require(sceRudpGetStatus(status.data(), 1) == 0);
    Require(status[0] == 0);
    for (std::size_t i = 1; i < status.size(); ++i) Require(status[i] == 0x5a);

    Require(sceRudpTerminate(first) == 0);
    Require(sceRudpTerminate(first) == invalidContextId);
    Require(sceRudpGetStatus(status.data(), statusSize) == 0);
    std::memcpy(&contexts, status.data() + currentContexts, sizeof(contexts));
    Require(contexts == 1);

    Require(sceRudpEnd() == 0);
    Require(sceRudpEnd() == notInitialized);
    Require(sceRudpTerminate(second) == notInitialized);
    Require(sceRudpGetStatus(status.data(), statusSize) == notInitialized);
    Require(sceRudpInit_nid_postfix(pool + 8, 0xC58) == 0);
    Require(sceRudpTerminate(second) == invalidContextId);
    Require(sceRudpEnd() == 0);
    return 0;
}
