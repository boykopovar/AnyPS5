#include "prx/libSceAgc/DcbDraw/include/Instancing.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetNumInstances(CommandBuffer* buf, std::uint32_t numInstances) {
    return Agc::Command::Emit(buf, 0x2fu, {numInstances}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetNumInstancesGetSize() {
    return 8;
}

uint32_t* APS5_VABI sceAgcDcbSetBaseIndirectArgs(CommandBuffer* buf, uint32_t shader_type, const volatile void* indirect_base_addr) {
 (void)buf;
 (void)shader_type;
 (void)indirect_base_addr;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}
