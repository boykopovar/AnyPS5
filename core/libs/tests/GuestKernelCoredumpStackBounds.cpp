#include "prx/libkernel/include/Api.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

alignas(16) std::array<std::byte, 0x40> process{};
constexpr int CoredumpInvalidArgument = static_cast<int>(0x81180000u);
constexpr int CoredumpNotRegistered = static_cast<int>(0x81180001u);
constexpr int CoredumpAlreadyRegistered = static_cast<int>(0x81180002u);

int APS5_VABI Handler(void*) {
    return 0;
}

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

} // namespace

int main() {
    const std::uint64_t processSize = process.size();
    const std::uint32_t processMagic = 0x4942524f;
    const std::uint32_t sdkVersion = 0x06500000;
    std::memcpy(process.data(), &processSize, sizeof(processSize));
    std::memcpy(process.data() + 8, &processMagic, sizeof(processMagic));
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(ProcessParametersFixture::Install(process.data(), process.size()));

    const uint64_t handlerAddr = reinterpret_cast<uint64_t>(&Handler);

    Require(sceCoredumpUnregisterCoredumpHandler() == CoredumpNotRegistered);

    Require(sceCoredumpRegisterCoredumpHandler(0, 0x4000, 0) == CoredumpInvalidArgument);
    Require(sceCoredumpRegisterCoredumpHandler(0, 0x8000, 0x1234) == CoredumpInvalidArgument);
    Require(sceCoredumpRegisterCoredumpHandler(0, 0x10000, 0) == CoredumpInvalidArgument);
    Require(sceCoredumpRegisterCoredumpHandler(0, 0x1fffffff, 0) == CoredumpInvalidArgument);
    Require(sceCoredumpRegisterCoredumpHandler(0, 0, 0) == CoredumpInvalidArgument);

    const std::size_t invalidLowStackSizes[] = {
        0, 1, 2, 0xff, 0x1000, 0x2000, 0x3000, 0x3ffe, 0x3fff
    };
    for (const std::size_t size : invalidLowStackSizes) {
        Require(sceCoredumpRegisterCoredumpHandler(handlerAddr, size, 0) == CoredumpInvalidArgument);
        Require(sceCoredumpRegisterCoredumpHandler(0, size, 0) == CoredumpInvalidArgument);
    }

    const std::size_t invalidHighStackSizes[] = {
        0x20000000,
        0x20000001,
        0x20001000,
        0x30000000,
        0x40000000,
        0x7fffffff,
        0x80000000,
        0xffffffff,
        SIZE_MAX - 1,
        SIZE_MAX
    };
    for (const std::size_t size : invalidHighStackSizes) {
        Require(sceCoredumpRegisterCoredumpHandler(handlerAddr, size, 0) == CoredumpInvalidArgument);
        Require(sceCoredumpRegisterCoredumpHandler(0, size, 0) == CoredumpInvalidArgument);
    }

    const std::size_t validStackSizes[] = {
        0x4000,
        0x4001,
        0x8000,
        0x10000,
        0x20000,
        0x40000,
        0x100000,
        0x1000000,
        0x10000000,
        0x1fffffff
    };
    for (const std::size_t size : validStackSizes) {
        const uint64_t contextValue = static_cast<uint64_t>(size) ^ 0xabcdef;
        Require(sceCoredumpRegisterCoredumpHandler(handlerAddr, size, contextValue) == 0);
        Require(sceCoredumpRegisterCoredumpHandler(handlerAddr, size, contextValue) == CoredumpAlreadyRegistered);
        Require(sceCoredumpRegisterCoredumpHandler(0, 0, 0) == CoredumpAlreadyRegistered);
        Require(sceCoredumpRegisterCoredumpHandler(0, 0x1000, 0) == CoredumpAlreadyRegistered);
        Require(sceCoredumpRegisterCoredumpHandler(0, 0x4000, 0) == CoredumpAlreadyRegistered);
        Require(sceCoredumpRegisterCoredumpHandler(0, 0x40000000, 0) == CoredumpAlreadyRegistered);
        Require(sceCoredumpRegisterCoredumpHandler(handlerAddr, 0x1000, 0) == CoredumpAlreadyRegistered);
        Require(sceCoredumpRegisterCoredumpHandler(handlerAddr, 0x40000000, 0) == CoredumpAlreadyRegistered);
        Require(sceCoredumpUnregisterCoredumpHandler() == 0);
        Require(sceCoredumpUnregisterCoredumpHandler() == CoredumpNotRegistered);
    }

    Require(sceCoredumpRegisterCoredumpHandler(handlerAddr, 0x8000, 0xbeefcafe) == 0);
    Require(sceCoredumpUnregisterCoredumpHandler() == 0);
    Require(sceCoredumpUnregisterCoredumpHandler() == CoredumpNotRegistered);

    return 0;
}
