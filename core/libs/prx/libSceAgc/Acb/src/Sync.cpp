#include "prx/libSceAgc/Acb/include/Sync.hpp"

#include "prx/libSceAgc/Command/include/Control.hpp"
#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcAcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t num_dwords) {
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    Agc::Command::CheckAddress(guestAddress, 4, __func__);
    Agc::Command::Require(num_dwords <= 0x3fffu, __func__, "conditional execution range is too large");
    return Agc::Command::Emit(buf, 0x22u,
                              {static_cast<std::uint32_t>(guestAddress) & ~3u,
                               static_cast<std::uint32_t>(guestAddress >> 32u), 0, num_dwords & 0x3fffu},
                              __func__);
}

uint32_t APS5_VABI sceAgcAcbCondExecGetSize(void) { return 20; }

std::uint32_t* APS5_VABI sceAgcAcbWaitRegMem(CommandBuffer* buf, std::uint8_t size, std::uint8_t compareFunction,
                                             std::uint8_t cachePolicy, const volatile void* address,
                                             std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles) {
    return Agc::Command::WriteWait(buf, size, compareFunction, 0, cachePolicy, address, reference, mask, pollCycles,
                                   __func__);
}

std::uint64_t APS5_VABI sceAgcAcbWaitOnAddressGetSize(std::uint8_t size) { return size == 0 ? 56 : 64; }

std::uint32_t* APS5_VABI sceAgcAcbEventWrite(CommandBuffer* buf, std::uint8_t eventType, const volatile void* address) {
    Agc::Command::CheckBits(eventType, 0x3fu, __func__);
    Agc::Command::Require(address == nullptr, __func__, "ACB event address is not supported");
    return Agc::Command::Emit(buf, 0x46u, {eventType | (eventType == 7 ? 0x400u : 0u)}, __func__);
}

std::uint64_t APS5_VABI sceAgcAcbEventWriteGetSize(std::uint8_t eventType) {
    (void)eventType;
    return 8;
}

std::uint32_t APS5_VABI sceAgcAcbQueueEndOfShaderActionGetSize() { return 32; }

std::uint32_t* APS5_VABI sceAgcAcbMemSemaphore(CommandBuffer* buf, std::uint8_t action, std::uint8_t clientCode,
                                               const volatile void* address) {
    const std::uint8_t operation = action != 0 ? 7u : 6u;
    const std::uint8_t signalType = clientCode & 1u;
    const std::uint8_t waitForMailbox = (clientCode >> 1u) & 1u;
    return Agc::Command::WriteMemSemaphore(buf, address, waitForMailbox, signalType, operation, __func__);
}
}
