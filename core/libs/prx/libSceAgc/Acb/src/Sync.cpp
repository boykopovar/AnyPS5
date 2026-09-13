#include "prx/libSceAgc/Acb/include/Sync.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcAcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t num_dwords) {
 (void)buf;
 (void)address;
 (void)num_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcAcbCondExecGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcAcbWaitRegMem(CommandBuffer* buf, std::uint8_t size, std::uint8_t compareFunction, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles) {
    return Agc::Command::WriteWait(buf, size, compareFunction, 0, cachePolicy, address, reference, mask, pollCycles, __func__);
}

std::uint64_t APS5_VABI sceAgcAcbWaitOnAddressGetSize(std::uint8_t size) {
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcAcbEventWrite(CommandBuffer* buf, std::uint8_t eventType, const volatile void* address) {
    Agc::Command::CheckBits(eventType, 0x3fu, __func__);
    Agc::Command::Require(address == nullptr, __func__, "ACB event address is not supported");
    return Agc::Command::Emit(buf, 0x46u, {eventType | (eventType == 7 ? 0x400u : 0u)}, __func__);
}

}
