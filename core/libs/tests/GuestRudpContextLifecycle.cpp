#include "SceTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

extern "C" {
using GuestRudpEventHandler = void(APS5_VABI*)(int, int, int, void*);
int APS5_VABI sceRudpInit_nid_postfix(void*, int);
int APS5_VABI sceRudpEnd();
int APS5_VABI sceRudpActivate(std::int32_t, const void*, std::uint32_t);
int APS5_VABI sceRudpEnableInternalIOThread(std::uint32_t, std::uint32_t);
int APS5_VABI sceRudpSetEventHandler(GuestRudpEventHandler, void*);
int APS5_VABI sceRudpCreateContext(GuestRudpEventHandler, void*, int*);
int APS5_VABI sceRudpBind(int, std::uint16_t, int, std::uint16_t);
int APS5_VABI sceRudpSetOption(int, int, const void*, std::uint32_t);
int APS5_VABI sceRudpGetOption(int, int, void*, std::uint32_t);
int APS5_VABI sceRudpInitiate(int, const void*, std::uint32_t, std::uint16_t);
int APS5_VABI sceRudpGetContextStatus(int, void*, std::uint32_t);
int APS5_VABI sceRudpWrite(int, const void*, std::uint32_t, int);
int APS5_VABI sceRudpRead(int, void*, std::uint32_t, int, void*);
int APS5_VABI sceRudpTerminate(int);
}

static void Require(bool value) { if (!value) std::abort(); }

static void APS5_VABI Handler(int, int, int, void*) {}

static void CheckOptions(int context) {
    constexpr int invalidArgument = static_cast<int>(0x80770004u);
    constexpr int invalidOption = static_cast<int>(0x80770005u);
    constexpr std::array<std::pair<int, std::uint32_t>, 16> defaults{{
        {1, 1346}, {2, 65536}, {3, 65536}, {4, 0}, {5, 1}, {6, 1}, {7, 1}, {8, 0},
        {9, 60000}, {10, 0}, {11, 30}, {15, 0}, {16, 0}, {17, 0}, {18, 0}, {19, 0},
    }};
    int other = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &other) == 0);
    std::array<unsigned char, 6> buffer{};
    for (const auto& [option, initial] : defaults) {
        std::uint32_t result = 0;
        Require(sceRudpGetOption(context, option, &result, sizeof(result)) == 0);
        Require(result == initial);
        for (std::uint32_t value : {0u, 1u, 0x80000000u, 0xffffffffu}) {
            buffer.fill(0xa5);
            std::memcpy(buffer.data() + 1, &value, sizeof(value));
            Require(sceRudpSetOption(context, option, buffer.data() + 1, sizeof(value)) == 0);
            buffer.fill(0xa5);
            Require(sceRudpGetOption(context, option, buffer.data() + 1, sizeof(value)) == 0);
            std::memcpy(&result, buffer.data() + 1, sizeof(result));
            Require(result == value && buffer.front() == 0xa5 && buffer.back() == 0xa5);
            Require(sceRudpGetOption(other, option, &result, sizeof(result)) == 0);
            Require(result == initial);
        }
        for (std::uint32_t length : {0u, 1u, 3u, 5u, 8u, std::numeric_limits<std::uint32_t>::max()}) {
            Require(sceRudpSetOption(context, option, &initial, length) == invalidArgument);
            buffer.fill(0xa5);
            Require(sceRudpGetOption(context, option, buffer.data() + 1, length) == invalidArgument);
            for (auto byte : buffer) Require(byte == 0xa5);
        }
        Require(sceRudpSetOption(context, option, nullptr, sizeof(result)) == invalidArgument);
        Require(sceRudpGetOption(context, option, nullptr, sizeof(result)) == invalidArgument);
        Require(sceRudpGetOption(context, option, &result, sizeof(result)) == 0);
        Require(result == 0xffffffffu);
    }
    for (int option : {std::numeric_limits<int>::min(), -1, 0, 20, std::numeric_limits<int>::max()}) {
        Require(sceRudpSetOption(context, option, nullptr, 0) == invalidOption);
        buffer.fill(0xa5);
        Require(sceRudpGetOption(context, option, buffer.data() + 1, 4) == invalidOption);
        for (auto byte : buffer) Require(byte == 0xa5);
    }
    for (int option : {12, 13, 14}) {
        std::uint32_t value = 0xa5a5a5a5u;
        bool threw = false;
        try {
            sceRudpGetOption(context, option, &value, sizeof(value));
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw && value == 0xa5a5a5a5u);
        for (std::uint32_t input : {0u, 1u, 0x80000000u, 0xffffffffu}) {
            buffer.fill(0xa5);
            std::memcpy(buffer.data() + 1, &input, sizeof(input));
            Require(sceRudpSetOption(context, option, buffer.data() + 1, sizeof(input)) == 0);
            Require(buffer.front() == 0xa5 && buffer.back() == 0xa5);
            for (int current : {context, other}) {
                threw = false;
                try {
                    sceRudpGetOption(current, option, &value, sizeof(value));
                } catch (const std::runtime_error&) {
                    threw = true;
                }
                Require(threw && value == 0xa5a5a5a5u);
            }
        }
        for (std::uint32_t length : {0u, 1u, 3u, 5u, 8u, std::numeric_limits<std::uint32_t>::max()}) {
            Require(sceRudpSetOption(context, option, &value, length) == invalidArgument);
        }
        Require(sceRudpSetOption(context, option, nullptr, sizeof(value)) == invalidArgument);
    }
    Require(sceRudpTerminate(other) == 0);
}

int main() {
    constexpr int notInitialized = static_cast<int>(0x80770001u);
    constexpr int invalidContextId = static_cast<int>(0x80770003u);
    constexpr int invalidOption = static_cast<int>(0x80770005u);
    constexpr int connRefused = static_cast<int>(0x8077000Au);
    constexpr int connReset = static_cast<int>(0x80770009u);
    alignas(16) static unsigned char pool[0x10000];

    Require(sceRudpActivate(-1, nullptr, 0) == static_cast<int>(0x80770001));

    Require(sceRudpEnableInternalIOThread(0, 0) == notInitialized);

    Require(sceRudpSetEventHandler(Handler, nullptr) == 0);

    Require(sceRudpBind(0, 0, 0, 0) == notInitialized);
    Require(sceRudpBind(12345, 0, 0, 0) == notInitialized);

    Require(sceRudpSetOption(0, 0, nullptr, 0) == notInitialized);
    Require(sceRudpSetOption(12345, 0, nullptr, 0) == notInitialized);
    Require(sceRudpGetOption(12345, 0, nullptr, 0) == notInitialized);

    Require(sceRudpInitiate(0, nullptr, 0, 0) == notInitialized);
    Require(sceRudpInitiate(12345, nullptr, 0, 0) == notInitialized);

    {
        unsigned char status[64] = {};
        Require(sceRudpGetContextStatus(0, status, sizeof(status)) == notInitialized);
        Require(sceRudpGetContextStatus(12345, status, sizeof(status)) == notInitialized);
    }

    Require(sceRudpWrite(0, nullptr, 0, 0) == notInitialized);
    Require(sceRudpWrite(12345, nullptr, 0, 0) == notInitialized);

    Require(sceRudpRead(0, nullptr, 0, 0, nullptr) == notInitialized);
    Require(sceRudpRead(12345, nullptr, 0, 0, nullptr) == notInitialized);

    Require(sceRudpInit_nid_postfix(pool, sizeof(pool)) == 0);

    Require(sceRudpActivate(-1, nullptr, 0) == static_cast<int>(0x80770004));

    Require(sceRudpEnableInternalIOThread(0, 0) == 0);
    Require(sceRudpEnableInternalIOThread(1024, 1) == 0);

    Require(sceRudpBind(12345, 0, 0, 0) == invalidContextId);

    Require(sceRudpSetOption(12345, 0, nullptr, 0) == invalidContextId);
    Require(sceRudpGetOption(12345, 0, nullptr, 0) == invalidContextId);
    Require(sceRudpGetOption(-1, 1, nullptr, 4) == invalidContextId);
    Require(sceRudpSetOption(-1, 1, nullptr, 4) == invalidContextId);

    Require(sceRudpInitiate(12345, nullptr, 0, 0) == invalidContextId);

    {
        unsigned char status[64] = {};
        Require(sceRudpGetContextStatus(12345, status, sizeof(status)) == invalidContextId);
    }

    Require(sceRudpWrite(12345, nullptr, 0, 0) == invalidContextId);

    Require(sceRudpRead(12345, nullptr, 0, 0, nullptr) == invalidContextId);

    int ctx = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &ctx) == 0);
    Require(ctx >= 0);

    Require(sceRudpActivate(ctx, nullptr, 0) == static_cast<int>(0x80770004));
    unsigned char address[34]{};
    Require(sceRudpActivate(-1, address, sizeof(address)) == static_cast<int>(0x80770004));
    bool activationUnsupported = false;
    try {
        sceRudpActivate(ctx, address, sizeof(address));
    } catch (const std::runtime_error&) {
        activationUnsupported = true;
    }
    Require(activationUnsupported);

    Require(sceRudpBind(ctx, 0, 0, 0) == 0);

    Require(sceRudpSetOption(ctx, 0, nullptr, 0) == invalidOption);
    CheckOptions(ctx);

    Require(sceRudpInitiate(ctx, nullptr, 0, 0) == connRefused);

    {
        unsigned char status[64];
        std::memset(status, 0x5a, sizeof(status));
        Require(sceRudpGetContextStatus(ctx, status, sizeof(status)) == 0);
        for (std::size_t i = 0; i < sizeof(status); ++i) Require(status[i] == 0);
    }
    Require(sceRudpGetContextStatus(ctx, nullptr, 64) == 0);

    Require(sceRudpWrite(ctx, nullptr, 0, 0) == connReset);

    Require(sceRudpRead(ctx, nullptr, 0, 0, nullptr) == connReset);

    Require(sceRudpSetEventHandler(Handler, nullptr) == 0);

    Require(sceRudpTerminate(ctx) == 0);
    Require(sceRudpGetOption(ctx, 1, nullptr, 4) == invalidContextId);
    Require(sceRudpSetOption(ctx, 1, nullptr, 4) == invalidContextId);
    Require(sceRudpEnd() == 0);
    Require(sceRudpGetOption(ctx, 1, nullptr, 4) == notInitialized);
    Require(sceRudpSetOption(ctx, 1, nullptr, 4) == notInitialized);
    Require(sceRudpInit_nid_postfix(pool, sizeof(pool)) == 0);
    Require(sceRudpGetOption(ctx, 1, nullptr, 4) == invalidContextId);
    Require(sceRudpCreateContext(Handler, nullptr, &ctx) == 0);
    CheckOptions(ctx);
    Require(sceRudpEnd() == 0);
    return 0;
}
