#include "prx/libSceAgc/Shader/include/CreateShader.hpp"

#include <cerrno>
#include <cstdio>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgc/Shader/include/ShaderUtils.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

extern "C" {

int APS5_VABI sceAgcCreateShader(Shader** dst, void* header, const volatile void* code) {
    constexpr auto fn = __func__;
    if (dst == nullptr) {
        throw std::runtime_error(std::string(fn) + ": dst is null");
    }
    if (!GuestWritable_nid_no_patch(dst, sizeof(*dst))) {
        throw std::runtime_error(std::string(fn) + ": dst is not writable");
    }
    if (header == nullptr) {
        throw std::runtime_error(std::string(fn) + ": header is null");
    }
    if (code == nullptr) {
        throw std::runtime_error(std::string(fn) + ": code is null");
    }

    auto* h = static_cast<Shader*>(header);

    if (h->file_header != ShaderRegs::SHADER_FILE_HEADER_MAGIC || h->version != ShaderRegs::SHADER_VERSION) {
        throw std::runtime_error(std::string(fn) + ": invalid shader header or version");
    }
    const auto base = reinterpret_cast<std::uint64_t>(code);
    if ((base & ShaderRegs::SHADER_BASE_ALIGN_MASK) != 0 || h->shader_size == 0 || (h->shader_size & 3u) != 0) {
        throw std::runtime_error(std::string(fn) + ": invalid shader code address or size");
    }
    std::uint32_t programOffset = 0;
    (void)GetProgramAddressRegisterOffset(h->type, programOffset);

    const auto previousBase = reinterpret_cast<std::uint64_t>(h->code);
    if (previousBase == 0) {
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
    }

    h->code = code;

    int result = PatchProgramAddressRegister(h->sh_registers, h->num_sh_registers, h->type, base, previousBase);
    if (result != 0) {
        return result;
    }

    AgcDriverRegisterShader_nid_postfix(h);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(dst, &h, sizeof(h));
    errno = savedErrno;
    if (!copied) {
        throw std::runtime_error(std::string(fn) + ": failed to copy dst pointer");
    }
    return 0;
}

}
