#include "prx/libSceAgc/DcbFlow/include/Sync.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "shader/recompilier/Recompiler.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t num_dwords) {
 (void)buf;
 (void)address;
 (void)num_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbCondExecGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbWaitRegMem(CommandBuffer* buf, std::uint8_t size, std::uint8_t compareFunction, std::uint8_t operation, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles) {
    return Agc::Command::WriteWait(buf, size, compareFunction, operation, cachePolicy, address, reference, mask, pollCycles, __func__);
}

uint32_t APS5_VABI sceAgcDcbWaitOnAddressGetSize(uint32_t size) {
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbEventWrite(CommandBuffer* buf, std::uint8_t eventType, const volatile void* address) {
    Agc::Command::CheckBits(eventType, 0x3fu, __func__);
    if ((eventType & 0xfeu) == 0x38u) {
        const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
        Agc::Command::CheckAddress(guestAddress, 8, __func__);
        return Agc::Command::Emit(buf, 0x46u, {0x100u | eventType, static_cast<std::uint32_t>(guestAddress), static_cast<std::uint32_t>(guestAddress >> 32u)}, __func__);
    }
    Agc::Command::Require(address == nullptr, __func__, "this event does not use an address");
    const auto eventIndex = eventType == 7 || eventType == 15 || eventType == 16 ? 0x400u : 0u;
    return Agc::Command::Emit(buf, 0x46u, {eventIndex | eventType}, __func__);
}

std::uint64_t APS5_VABI sceAgcDcbEventWriteGetSize(std::uint8_t eventType) {
    (void)eventType;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbStallCommandBufferParser(CommandBuffer* buf) {
    return Agc::Command::Emit(buf, 0x42u, {0}, __func__);
}

}
