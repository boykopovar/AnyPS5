#include "prx/libSceAgc/Misc/include/ShaderFusion.hpp"
#include "prx/libSceAgcDriver/tests/execution/VulkanTestDevice.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t SCodeEnd = 0xbf9f0000u;
constexpr std::uint32_t SNop = 0xbf800000u;
constexpr std::uint32_t SSetpcS6 = 0xbe802006u;
constexpr std::uint32_t SSwappcNullS6 = 0xbefd2106u;
constexpr std::uint32_t SMovS0 = 0xbe800380u;
constexpr std::uint32_t VMovV0 = 0x7e000280u;
constexpr std::uint32_t SEndpgm = 0xbf810000u;
constexpr int InvalidShaderHalves = static_cast<int>(0x8a6c0008u);
constexpr std::uint32_t GsStage = 1u << 22u;
constexpr std::uint32_t HsStage = 1u << 21u;

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    try {
        action();
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::exception& error) {
        Require(error.what()[0] != '\0', "empty exception message");
        return;
    }
    Testing::Fail("expected an exception");
}

std::uint32_t word(const void* memory, std::size_t index) {
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const std::uint8_t*>(memory) + index * 4, sizeof(value));
    return value;
}

const ShaderRegister* find(const Shader& shader, std::uint32_t offset) {
    for (std::uint32_t i = 0; i < shader.num_sh_registers; ++i) {
        if (shader.sh_registers[i].offset == offset) return &shader.sh_registers[i];
    }
    return nullptr;
}

struct Halves {
    std::vector<std::uint32_t> frontCode;
    std::array<std::uint32_t, 3> backCode{SMovS0, SEndpgm, SCodeEnd};
    std::array<ShaderRegister, 4> frontSh{{{0x080u, 0x1234u}, {0x08Au, (2u << 6u) | 3u}, {0x08Bu, (6u << 1u) | (1u << 7u)}, {0x0B2u, 0x55u}}};
    std::array<ShaderRegister, 2> frontCx{{{0x29Bu, 7u}, {0x2D5u, 0x24u}}};
    std::array<ShaderRegister, 6> backSh{{{0x088u, 0u}, {0x089u, 0u}, {0x08Au, (1u << 24u) | (1u << 6u) | 5u}, {0x08Bu, (2u << 1u) | (1u << 7u) | (1u << 15u)}, {0x0C8u, 0u}, {0x0C9u, 0u}}};
    std::array<ShaderRegister, 1> backCx{{{0x29Bu, 2u}}};
    std::array<ShaderSemantic, 2> inputs{};
    std::array<ShaderSemantic, 1> outputs{};
    std::array<std::uint16_t, 2> direct{3u, 9u};
    std::array<ShaderSharp, 1> sharps{};
    ShaderUserData userData{};
    ShaderSpecialRegs frontSpecials{};
    ShaderSpecialRegs backSpecials{};
    Shader front{};
    Shader back{};

    explicit Halves(std::uint32_t terminator = SSetpcS6) {
        frontCode = {SMovS0, VMovV0, terminator, SCodeEnd, SCodeEnd};
        const auto codeBytes = static_cast<std::uint32_t>(frontCode.size() * 4);
        std::array<std::uint32_t, 2> magic{};
        std::memcpy(magic.data(), "barefoot", 8);
        frontCode.insert(frontCode.end(), {magic[0], magic[1], 0xabcdu, 0u, 1u, codeBytes, 0u});
        inputs[0].semantic = 1;
        inputs[1].semantic = 2;
        outputs[0].semantic = 3;
        sharps[0].offset_dw = 12;
        userData.direct_resource_offset = direct.data();
        userData.direct_resource_count = static_cast<std::uint16_t>(direct.size());
        userData.sharp_resource_offset[1] = sharps.data();
        userData.sharp_resource_count[1] = 1;
        userData.eud_size_dw = 4;
        frontSpecials.vgt_shader_stages_en.value = GsStage;
        backSpecials.vgt_shader_stages_en.value = GsStage;
        backSpecials.ge_cntl.value = 0x77u;
        front.file_header = ShaderRegs::SHADER_FILE_HEADER_MAGIC;
        front.version = ShaderRegs::SHADER_VERSION;
        front.code = frontCode.data();
        front.shader_size = static_cast<std::uint32_t>(frontCode.size() * 4);
        front.header_size = sizeof(Shader);
        front.sh_registers = frontSh.data();
        front.num_sh_registers = static_cast<std::uint8_t>(frontSh.size());
        front.cx_registers = frontCx.data();
        front.num_cx_registers = static_cast<std::uint8_t>(frontCx.size());
        front.input_semantics = inputs.data();
        front.num_input_semantics = static_cast<std::uint32_t>(inputs.size());
        front.specials = &frontSpecials;
        front.scratch_size_dw_per_thread = 16;
        front.type = static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::GsFront);
        back.file_header = ShaderRegs::SHADER_FILE_HEADER_MAGIC;
        back.version = ShaderRegs::SHADER_VERSION;
        back.code = backCode.data();
        back.shader_size = static_cast<std::uint32_t>(backCode.size() * 4);
        back.header_size = sizeof(Shader);
        back.sh_registers = backSh.data();
        back.num_sh_registers = static_cast<std::uint8_t>(backSh.size());
        back.cx_registers = backCx.data();
        back.num_cx_registers = static_cast<std::uint8_t>(backCx.size());
        back.output_semantics = outputs.data();
        back.num_output_semantics = static_cast<std::uint16_t>(outputs.size());
        back.user_data = &userData;
        back.specials = &backSpecials;
        back.special_sizes_bytes = sizeof(ShaderSpecialRegs);
        back.scratch_size_dw_per_thread = 8;
        back.type = static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::GsBack);
    }
};

std::uint8_t* aligned(std::vector<std::uint8_t>& storage, std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    return storage.data() + ((alignment - address % alignment) % alignment);
}

void VerifyFusion(std::size_t misalignment, std::uint32_t terminator = SSetpcS6) {
    Halves halves(terminator);
    SizeAlign size{};
    Require(sceAgcGetFusedShaderSize_0080(&size, &halves.front, &halves.back) == 0, "fused size query failed");
    Require(size.m_align == 8, "fused shaders are not 256-byte aligned");
    Require(size.m_size > 256 + 255 + halves.back.shader_size + sizeof(Shader), "fused size does not cover the alignment padding, code and header");
    std::vector<std::uint8_t> storage(size.m_size + 512, 0xcd);
    auto* scratch = aligned(storage, 256) + misalignment;
    Shader fused{};
    Require(sceAgcFuseShaderHalves_0200(&fused, &halves.front, &halves.back, scratch) == 0, "fusion failed");
    for (std::size_t i = 0; i < 16; ++i) Require(scratch[size.m_size + i] == 0xcd, "fusion wrote past the reported size");
    auto* memory = scratch + (256 - misalignment) % 256;
    const auto base = reinterpret_cast<std::uintptr_t>(memory);
    const auto scratchBase = reinterpret_cast<std::uintptr_t>(scratch);
    Require(fused.type == static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::Gs), "fused shader is not a GS");
    Require(fused.code == memory && fused.shader_size == 256 + halves.back.shader_size, "fused code placement is wrong");
    Require(word(memory, 0) == SMovS0 && word(memory, 1) == VMovV0, "front program was not copied");
    for (std::size_t i = 2; i < 64; ++i) Require(word(memory, i) == SNop, "front program does not fall through to the back half");
    Require(std::memcmp(memory + 256, halves.backCode.data(), halves.back.shader_size) == 0, "back program was not copied");
    const auto* backLo = find(fused, 0x088u);
    const auto* frontLo = find(fused, 0x0C8u);
    Require(backLo != nullptr && backLo->value == static_cast<std::uint32_t>((base + 256) >> 8u), "back program address was not patched");
    Require(frontLo != nullptr && frontLo->value == static_cast<std::uint32_t>(base >> 8u), "fused program address was not patched");
    Require(find(fused, 0x08Au)->value == ((1u << 24u) | (2u << 6u) | 5u), "RSRC1 register counts were not merged");
    Require(find(fused, 0x08Bu)->value == ((6u << 1u) | (1u << 7u) | (1u << 15u)), "RSRC2 user SGPRs were not taken from the front half");
    Require(find(fused, 0x0B2u) != nullptr && find(fused, 0x0B2u)->value == 0x55u, "front-only SH register was lost");
    Require(find(fused, 0x080u) == nullptr, "front program checksum was kept");
    Require(fused.num_sh_registers == 7 && fused.num_cx_registers == 2, "fused register counts are wrong");
    Require(fused.cx_registers[0].offset == 0x29Bu && fused.cx_registers[0].value == 2u && fused.cx_registers[1].offset == 0x2D5u, "context registers were not merged");
    Require(fused.num_input_semantics == 2 && fused.input_semantics[1].semantic == 2, "front inputs were not kept");
    Require(fused.num_output_semantics == 1 && fused.output_semantics[0].semantic == 3, "back outputs were not kept");
    Require(fused.scratch_size_dw_per_thread == 16, "scratch size is not the larger half's");
    Require(fused.specials != nullptr && fused.specials != halves.back.specials && fused.specials->ge_cntl.value == 0x77u, "back specials were not copied");
    const auto inside = [&](const void* pointer) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        return address >= scratchBase && address < scratchBase + size.m_size;
    };
    Require(inside(fused.sh_registers) && inside(fused.cx_registers) && inside(fused.specials) && inside(fused.input_semantics) && inside(fused.output_semantics), "fused tables are not in the fused memory");
    Require(inside(fused.user_data) && inside(fused.user_data->direct_resource_offset) && inside(fused.user_data->sharp_resource_offset[1]), "fused user data is not in the fused memory");
    Require(fused.user_data->direct_resource_count == 2 && fused.user_data->direct_resource_offset[1] == 9u && fused.user_data->sharp_resource_offset[1][0].offset_dw == 12 && fused.user_data->sharp_resource_offset[0] == nullptr && fused.user_data->eud_size_dw == 4, "fused user data is wrong");
    const auto headerOffset = static_cast<std::size_t>(size.m_size) - 255 - fused.header_size;
    Require(headerOffset % alignof(Shader) == 0 && headerOffset >= fused.shader_size, "fused header is misplaced");
    const auto* header = reinterpret_cast<const Shader*>(memory + headerOffset);
    Require(header->code == fused.code && header->sh_registers == fused.sh_registers && header->user_data == fused.user_data && header->header_size == fused.header_size && header->shader_size == fused.shader_size && header->type == fused.type && header->num_sh_registers == fused.num_sh_registers, "fused memory has no self-contained header");
}

void VerifyRejections() {
    Halves halves;
    SizeAlign size{};
    Shader fused{};
    ExpectFailure([&] { sceAgcGetFusedShaderSize_0080(nullptr, &halves.front, &halves.back); });
    ExpectFailure([&] { sceAgcFuseShaderHalves_0200(&fused, &halves.front, &halves.back, nullptr); });
    {
        Halves swapped;
        Require(sceAgcGetFusedShaderSize_0080(&size, &swapped.back, &swapped.front) == InvalidShaderHalves, "swapped halves were accepted by the size query");
        Require(sceAgcFuseShaderHalves_0200(&fused, &swapped.back, &swapped.front, nullptr) == InvalidShaderHalves, "swapped halves were fused");
    }
    {
        Halves stages;
        stages.backSpecials.vgt_shader_stages_en.value = 0;
        std::vector<std::uint8_t> storage(4096);
        Require(sceAgcFuseShaderHalves_0200(&fused, &stages.front, &stages.back, storage.data()) == InvalidShaderHalves, "halves with different GS stages were fused");
    }
    {
        Halves noJump;
        noJump.frontCode[2] = SNop;
        ExpectFailure([&] { sceAgcGetFusedShaderSize_0080(&size, &noJump.front, &noJump.back); });
    }
    {
        Halves noTrailer;
        noTrailer.frontCode[5] = 0;
        ExpectFailure([&] { sceAgcGetFusedShaderSize_0080(&size, &noTrailer.front, &noTrailer.back); });
    }
    {
        Halves hardware;
        hardware.frontSh[2].value |= 1u << 16u;
        ExpectFailure([&] { sceAgcGetFusedShaderSize_0080(&size, &hardware.front, &hardware.back); });
    }
    {
        Halves constants;
        constants.back.embedded_constant_buffer_size_dqw = 1;
        ExpectFailure([&] { sceAgcGetFusedShaderSize_0080(&size, &constants.front, &constants.back); });
    }
}

void VerifyHullHalves() {
    std::array<std::uint32_t, 2> frontCode{SEndpgm, SCodeEnd};
    std::array<std::uint32_t, 2> backCode{SEndpgm, SCodeEnd};
    std::array<ShaderRegister, 4> frontSh{{{0x100u, 0x11u}, {0x100u, 0x22u}, {0x10Au, 7u}, {0x10Bu, 6u << 1u}}};
    std::array<ShaderRegister, 6> backSh{{{0x100u, 0u}, {0x100u, 0u}, {0x10Au, 3u}, {0x10Bu, 2u << 1u}, {0x148u, 0u}, {0x149u, 0xab00u}}};
    ShaderSpecialRegs specials{};
    specials.vgt_shader_stages_en.value = HsStage;
    Shader front{};
    front.code = frontCode.data();
    front.sh_registers = frontSh.data();
    front.num_sh_registers = static_cast<std::uint8_t>(frontSh.size());
    front.specials = &specials;
    front.type = static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::HsFront);
    Shader back{};
    back.code = backCode.data();
    back.sh_registers = backSh.data();
    back.num_sh_registers = static_cast<std::uint8_t>(backSh.size());
    back.specials = &specials;
    back.type = static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::HsBack);
    SizeAlign size{};
    Require(sceAgcGetFusedShaderSize_0080(&size, &front, &back) == 0 && size.m_size == backSh.size() * sizeof(ShaderRegister) && size.m_align == 4, "hull size query changed");
    std::vector<ShaderRegister> scratch(backSh.size());
    Shader fused{};
    Require(sceAgcFuseShaderHalves_0200(&fused, &front, &back, scratch.data()) == 0, "hull fusion failed");
    Require(fused.type == static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::Hs) && fused.code == back.code && fused.sh_registers == scratch.data() && fused.user_data == nullptr, "hull halves were not fused at the register level");
    const auto address = reinterpret_cast<std::uint64_t>(front.code);
    Require(scratch[0].value == 0x11u && scratch[1].value == 0x22u, "hull checksums were not taken from the front half");
    Require(scratch[2].value == 7u && scratch[3].value == (6u << 1u), "hull resource registers were not merged");
    Require(scratch[4].value == static_cast<std::uint32_t>(address >> 8u) && scratch[5].value == (0xab00u | static_cast<std::uint32_t>((address >> 40u) & 0xffu)), "hull local program address does not point at the front half");
    Require(backSh[4].value == 0u, "hull fusion wrote the back half's registers");
}

}

namespace {

const Testing::Case alignedScratch{"FuseShaderHalves_AlignedScratch_FusesWithJumpToBack", [] {
    SharedVulkanTestDevice();
    VerifyFusion(0);
}};

const Testing::Case misaligned{"FuseShaderHalves_ScratchOffset160_FusesWithJumpToBack", [] {
    SharedVulkanTestDevice();
    VerifyFusion(0xa0);
}};

const Testing::Case smallOffset{"FuseShaderHalves_ScratchOffset4_FusesWithJumpToBack", [] {
    SharedVulkanTestDevice();
    VerifyFusion(4);
}};

const Testing::Case swappc{"FuseShaderHalves_SwappcTerminator_FusesWithJumpToBack", [] {
    SharedVulkanTestDevice();
    VerifyFusion(0, SSwappcNullS6);
}};

const Testing::Case rejections{"FuseShaderHalves_InvalidHalves_AreRejected", [] {
    SharedVulkanTestDevice();
    VerifyRejections();
}};

const Testing::Case hull{"FuseShaderHalves_HullHalves_MergesRegisters", [] {
    SharedVulkanTestDevice();
    VerifyHullHalves();
}};

} // namespace

int main(int argc, char** argv) {
    const int result = Testing::Run(argc, argv);
    LibcRunShutdown_nid_postfix();
    return result;
}
