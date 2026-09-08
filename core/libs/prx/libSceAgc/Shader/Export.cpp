#include <cstdint>
#include <cstddef>
#include "SceShaders.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceAgcCreateShader(Shader** dst, void* header, const volatile void* code){
 (void)dst;
 (void)header;
 (void)code;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAgcCreatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, const Shader* hs, const Shader* gs, uint32_t prim_type){
 (void)cx_regs;
 (void)uc_regs;
 (void)hs;
 (void)gs;
 (void)prim_type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAgcUpdatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, uint32_t prim_type){
 (void)cx_regs;
 (void)uc_regs;
 (void)prim_type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAgcCreateInterpolantMapping(ShaderRegister* regs, const Shader* gs, const Shader* ps){
 (void)regs;
 (void)gs;
 (void)ps;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}
}
