#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

struct SizeAlign {
 uint64_t m_size;
 size_t m_align;
};

extern "C" {

uint32_t APS5_VABI sceAgcGetPacketSize(uint32_t* packet) {
 (void)packet;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* APS5_VABI sceAgcGetRegisterDefaults2(uint32_t ver) {
 (void)ver;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

APS5_EXPORT("fd5Bp5tGTgo", sceAgcUnknownFuseShaderHalves);
int APS5_VABI sceAgcUnknownFuseShaderHalves(Shader* fused_result, const Shader* front, const Shader* back, void* scratch_mem) {
    (void)fused_result;
    (void)front;
    (void)back;
    (void)scratch_mem;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("dolOmWH+huQ", sceAgcUnknownGetFusedShaderSize);
int APS5_VABI sceAgcUnknownGetFusedShaderSize(SizeAlign* dst, const Shader* front, const Shader* back) {
    (void)dst;
    (void)front;
    (void)back;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("V++UgBtQhn0", sceAgcGetDataPacketPayloadAnotherAddress);
int APS5_VABI sceAgcGetDataPacketPayloadAnotherAddress(uint32_t** addr, uint32_t* cmd, int type) {
    (void)addr;
    (void)cmd;
    (void)type;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("k0E7vkgqAuE", sceAgcCreateInterpolantMappingVsPs);
int APS5_VABI sceAgcCreateInterpolantMappingVsPs(ShaderRegister* regs, const Shader* vs, const Shader* ps) {
    (void)regs;
    (void)vs;
    (void)ps;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void* APS5_VABI sceAgcGetRegisterDefaults2Internal(uint32_t ver) {
 (void)ver;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI sceAgcGetDataPacketPayloadAddress(uint32_t** addr, uint32_t* cmd, int type) {
 (void)addr;
 (void)cmd;
 (void)type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSuspendPoint(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcInit_nid_postfix(uint32_t* state, uint32_t ver) {
 (void)state;
 (void)ver;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}
}
