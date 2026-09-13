#include "prx/libSceAgc/Shader/include/CreateShader.hpp"

#include <cstdio>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "prx/libSceAgc/Shader/include/ShaderUtils.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

extern "C" {

int APS5_VABI sceAgcCreateShader(Shader** dst, void* header, const volatile void* code) {
    constexpr auto fn = __func__;
    if (dst == nullptr) {
        throw std::runtime_error(std::string(fn) + ": dst is null");
    }
    if (header == nullptr) {
        throw std::runtime_error(std::string(fn) + ": header is null");
    }
    if (code == nullptr) {
        throw std::runtime_error(std::string(fn) + ": code is null");
    }

    auto* h = static_cast<Shader*>(header);

    ResolveRelativePtr(h->cx_registers);
    ResolveRelativePtr(h->sh_registers);
    ResolveRelativePtr(h->user_data);
    ResolveRelativePtr(h->specials);
    ResolveRelativePtr(h->input_semantics);
    ResolveRelativePtr(h->output_semantics);

    if (h->user_data != nullptr) {
        ResolveRelativePtr(h->user_data->direct_resource_offset);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[0]);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[1]);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[2]);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[3]);
    }

    h->code = code;

    if (h->file_header != ShaderRegs::SHADER_FILE_HEADER_MAGIC) {
        throw std::runtime_error(std::string(fn) + ": invalid file_header magic");
    }
    if (h->version != ShaderRegs::SHADER_VERSION) {
        throw std::runtime_error(std::string(fn) + ": unsupported shader version");
    }

    const auto base = reinterpret_cast<std::uint64_t>(code);

    if ((base & ShaderRegs::SHADER_BASE_ALIGN_MASK) != 0) {
        throw std::runtime_error(std::string(fn) + ": code address violates alignment mask");
    }

    int result = PatchProgramAddressRegister(h->sh_registers, h->num_sh_registers, h->type, base);
    if (result != 0) {
        return result;
    }

    *dst = h;
    APS5_LOG_OUT("OK type=%u sh_regs=%u shader_size=%u", h->type, h->num_sh_registers, h->shader_size);
    return 0;
}

}
