#include "prx/libSceAgc/DcbFlow/include/Control.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "shader/recompilier/Recompiler.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbJump(CommandBuffer* buf, uint8_t mode, uint8_t cache_policy, const uint32_t* target, uint32_t size_in_dwords) {
 (void)buf;
 (void)mode;
 (void)cache_policy;
 (void)target;
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbJumpGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, uint32_t op, uint32_t state) {
 (void)buf;
 (void)op;
 (void)state;
 const std::array<std::uint32_t, 1> code = {0xbf810000u};
 const std::array<std::uint32_t, 1> capabilities = {1u};
 const std::array<ShaderRecompiler::RegisterValue, 3> shaderRegisters = {{{0x207u, 1u}, {0x208u, 1u}, {0x209u, 1u}}};
 const std::array<ShaderRecompiler::MemoryRegion, 1> memory = {{{0x10000u, std::as_bytes(std::span(code))}}};
 const ShaderRecompiler::RecompileRequest request{
     {ShaderRecompiler::ShaderStage::Compute, 0x10000u, code, 0, {}},
     {64, 0, {}, shaderRegisters, {}, {}, memory},
     {0x00403000u, 0x00010600u, 64, capabilities, {}, {1024, 1024, 64}, 1024, 32768},
     {0, 0, 0, 0}
 };
 (void)ShaderRecompiler::Recompile(request);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbRewind(CommandBuffer* buf, uint32_t initial_state) {
 (void)buf;
 (void)initial_state;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbRewindGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer* buf, uint32_t video_out_handle, uint32_t display_buffer_index) {
 (void)buf;
 (void)video_out_handle;
 (void)display_buffer_index;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}
