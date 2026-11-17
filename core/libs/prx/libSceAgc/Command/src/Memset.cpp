#include "prx/libSceAgc/include/Agc.hpp"
#include "prx/libSceAgc/Cb/include/Registers.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/Shader/include/CreateShader.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstring>

namespace {

alignas(256) constexpr std::array<std::uint32_t, 21> FillCode{
    0xbf068008u, 0xbf850012u, 0x7e0a0209u, 0x340a0a88u, 0x4a000105u, 0x7e020204u, 0x7e040205u,
    0x7e060206u, 0x7e080207u, 0xbe8a0308u, 0x7d880002u, 0xbe8c246au, 0xe0782000u, 0x80000100u,
    0xbefe040cu, 0x4a0000ffu, 0x00001000u, 0x808a810au, 0xbf07800au, 0xbf85fff6u, 0xbf810000u
};

constexpr std::uint32_t FillChecksum = [] {
    std::uint32_t hash = 2166136261u;
    for (const auto word : FillCode) {
        for (unsigned shift = 0; shift < 32; shift += 8) hash = (hash ^ ((word >> shift) & 0xffu)) * 16777619u;
    }
    return hash;
}();

template <typename T>
T* Relative(T* target, const void* field) {
    return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(target) - reinterpret_cast<std::uintptr_t>(field));
}

struct FillProgram {
    Shader header{};
    std::array<ShaderRegister, 9> registers{{
        {0x20cu, 0u}, {0x20du, 0u}, {0x22au, FillChecksum}, {0x212u, 0x602c0041u}, {0x213u, 0x92u},
        {0x228u, 0u}, {0x207u, 256u}, {0x208u, 1u}, {0x209u, 1u}
    }};
    ShaderUserData userData{};
    ShaderSpecialRegs specials{};

    FillProgram() {
        header.file_header = ShaderRegs::SHADER_FILE_HEADER_MAGIC;
        header.version = ShaderRegs::SHADER_VERSION;
        header.header_size = sizeof(FillProgram);
        header.shader_size = sizeof(FillCode);
        header.type = static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::Cs);
        header.num_sh_registers = static_cast<std::uint8_t>(registers.size());
        header.sh_registers = Relative(registers.data(), &header.sh_registers);
        header.user_data = Relative(&userData, &header.user_data);
        header.specials = Relative(&specials, &header.specials);
        userData.srt_size_dw = 9;
        specials.dispatch_modifier = 1;
        Shader* created = nullptr;
        Agc::Command::Require(sceAgcCreateShader(&created, &header, FillCode.data()) == 0 && created == &header, "sceAgcCbMemsetExclusive", "cannot create the fill shader");
    }
};

FillProgram& Program() {
    static FillProgram program;
    return program;
}

} // namespace

extern "C" std::uint32_t* APS5_VABI sceAgcCbMemsetExclusive(CommandBuffer* buffer, void* destination, const void* pattern, std::uint64_t size) {
    constexpr auto function = __func__;
    Agc::Command::Require(buffer != nullptr, function, "null command buffer");
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(pattern), 1, function);
    auto& program = Program();
    auto* first = sceAgcCbSetShRegistersDirect(buffer, program.header.sh_registers, program.header.num_sh_registers);
    const auto address = reinterpret_cast<std::uint64_t>(destination);
    std::array<std::uint32_t, 9> data{
        static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | 0x100000u,
        static_cast<std::uint32_t>(size >> 4u), 0x4bfacu, 0u, 0u, 0u, 0u,
        static_cast<std::uint32_t>((size + 0xffffu) >> 16u)
    };
    std::memcpy(data.data() + 4, pattern, 16);
    sceAgcCbSetShRegisterRangeDirect(buffer, 0x240u, data.data(), static_cast<std::uint32_t>(data.size()));
    const auto mask = (static_cast<std::uint32_t>(sceKernelGetMainSocId()) & ~0x1fu) == 0x840fc0u ? 0x00ff00ffu : 0x0003fcffu;
    Agc::Command::Emit(buffer, 0x76u, {0x216u, mask}, function);
    Agc::Command::Emit(buffer, 0x76u, {0x217u, mask}, function);
    const auto initiator = (program.specials.dispatch_modifier & 0xa038u) | static_cast<std::uint32_t>(sceAgcDriverGetTraceInitiator()) | 0x41u;
    Agc::Command::Emit(buffer, 0x15u, {16u, 1u, 1u, initiator}, function);
    Agc::Command::Emit(buffer, 0x76u, {0x216u, 0xffffffffu}, function);
    Agc::Command::Emit(buffer, 0x76u, {0x217u, 0xffffffffu}, function);
    return first;
}
