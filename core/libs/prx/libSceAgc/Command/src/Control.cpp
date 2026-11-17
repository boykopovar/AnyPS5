#include "prx/libSceAgc/Command/include/Control.hpp"
#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"

namespace Agc::Command {

std::uint32_t* WriteJump(CommandBuffer* buffer, bool compute, std::uint8_t chain, std::uint8_t cachePolicy, const std::uint32_t* target, std::uint32_t sizeInDwords, const char* function) {
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(target);
    const auto control = (compute ? 0x0f800000u : 0x0f200000u) | ((static_cast<std::uint32_t>(cachePolicy) & 3u) << 28u) | ((static_cast<std::uint32_t>(chain) & 1u) << 20u) | (sizeInDwords & 0xfffffu);
    return Emit(buffer, 0x3fu, {static_cast<std::uint32_t>(guestAddress) & ~3u, static_cast<std::uint32_t>(guestAddress >> 32u), control}, function);
}

std::uint32_t* WriteRewind(CommandBuffer* buffer, std::uint32_t initialState, const char* function) {
    return Emit(buffer, 0x59u, {(initialState & 1u) << 31u}, function);
}

std::uint32_t* WritePredication(CommandBuffer* buffer, std::uint8_t condition, std::uint8_t operation, std::uint8_t waitOperation, const volatile void* address, const char* function) {
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    const auto control = ((static_cast<std::uint32_t>(condition) & 1u) << 8u) | ((static_cast<std::uint32_t>(waitOperation) & 1u) << 12u) | ((static_cast<std::uint32_t>(operation) & 7u) << 16u);
    return Emit(buffer, 0x20u, {control, static_cast<std::uint32_t>(guestAddress) & ~0xfu, static_cast<std::uint32_t>(guestAddress >> 32u)}, function);
}

std::uint32_t* WriteMemSemaphore(CommandBuffer* buffer, const volatile void* address, std::uint8_t waitForMailbox, std::uint8_t signalType, std::uint8_t operation, bool asyncCompute, const char* function) {
    const auto prefix = sceAgcDriverUserDataGetPacketSize(4);
    const auto suffix = sceAgcDriverUserDataGetPacketSize(0);
    if (!TryReserve(buffer, prefix + suffix + 4, function)) return nullptr;
    auto* result = Allocate(buffer, prefix + 4 + ((operation == 6 || operation == 7) ? suffix : 0), function);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    const auto low = static_cast<std::uint32_t>(guestAddress);
    const auto high = static_cast<std::uint32_t>(guestAddress >> 32u);
    const auto marker = (operation == 6 ? 0xc9010000u : 0xc9020000u) | (high & 0xffffu);
    sceAgcDriverUserDataWritePacket(result, 1, marker, &low, 4);
    auto* semaphore = result + prefix;
    if (operation == 6) {
        semaphore[0] = Header(0x10u, suffix);
        semaphore += suffix;
    }
    semaphore[0] = Header(0x39u, 4);
    semaphore[1] = low & ~7u;
    semaphore[2] = high;
    semaphore[3] = (static_cast<std::uint32_t>(operation & 7u) << 29u) |
        (static_cast<std::uint32_t>(signalType & 1u) << 20u) | (static_cast<std::uint32_t>(waitForMailbox & 1u) << 16u);
    if (operation == 7) {
        sceAgcDriverUserDataWritePacket(semaphore + 4, asyncCompute ? 1u : 0u, 0xc9000000u, nullptr, 0);
    }
    return result;
}

}
