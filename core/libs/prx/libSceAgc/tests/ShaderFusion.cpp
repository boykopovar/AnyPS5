#include "prx/libSceAgc/Misc/include/ShaderFusion.hpp"
#include "prx/libSceAgcDriver/tests/execution/VulkanTestDevice.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "Recompiler.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t SCodeEnd = 0xbf9f0000u;
constexpr std::uint32_t SNop = 0xbf800000u;
constexpr std::uint32_t SGetpcS60 = 0xbebc1f00u;
constexpr std::uint32_t SMovS0Literal = 0xbe8003ffu;
constexpr std::uint32_t SAddS60Literal = 0x803c3cffu;
constexpr std::uint32_t SAddcS61Zero = 0x823d3d80u;
constexpr std::uint32_t SSetpcS6 = 0xbe802006u;
constexpr std::uint32_t SSwappcNullS6 = 0xbefd2106u;
constexpr std::uint32_t SMovS0 = 0xbe800380u;
constexpr std::uint32_t VMovV0 = 0x7e000280u;
constexpr std::uint32_t SEndpgm = 0xbf810000u;
constexpr int InvalidShaderHalves = static_cast<int>(0x8a6c0008u);
constexpr std::uint32_t GsStage = 1u << 22u;
constexpr std::uint32_t HsStage = 1u << 21u;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::exception& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
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
    std::vector<std::uint32_t> backCode{SMovS0, SEndpgm, SCodeEnd};
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

void testFusion(std::size_t misalignment, std::uint32_t terminator = SSetpcS6) {
    Halves halves(terminator);
    SizeAlign size{};
    check(sceAgcGetFusedShaderSize_0080(&size, &halves.front, &halves.back) == 0, "fused size query failed");
    check(size.m_size == 6 * sizeof(ShaderRegister) && size.m_align == 4, "fused size is not the back half's register list");
    SizeAlign unversioned{~0ull, ~std::size_t{0}};
    check(sceAgcGetFusedShaderSize(&unversioned, &halves.front, &halves.back) == 0, "unversioned fused size query failed");
    check(unversioned.m_size == size.m_size && unversioned.m_align == (~std::size_t{0} << 32u | 4u), "unversioned fused size must store a 32-bit alignment");
    std::vector<std::uint8_t> storage(size.m_size + 512, 0xcd);
    auto* scratch = aligned(storage, 256) + misalignment;
    Shader fused{};
    check(sceAgcFuseShaderHalves_0200(&fused, &halves.front, &halves.back, scratch) == 0, "fusion failed");
    for (std::size_t i = 0; i < 16; ++i) check(scratch[size.m_size + i] == 0xcd, "fusion wrote past the reported size");
    const auto base = reinterpret_cast<std::uintptr_t>(fused.code);
    const auto* memory = reinterpret_cast<const std::uint8_t*>(base);
    check(fused.type == static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::Gs), "fused shader is not a GS");
    check(base % 256 == 0 && fused.shader_size == 256 + halves.back.shader_size, "fused code placement is wrong");
    check(word(memory, 0) == SMovS0 && word(memory, 1) == VMovV0, "front program was not copied");
    for (std::size_t i = 2; i < 64; ++i) check(word(memory, i) == SNop, "front program does not fall through to the back half");
    check(std::memcmp(memory + 256, halves.backCode.data(), halves.back.shader_size) == 0, "back program was not copied");
    check(reinterpret_cast<std::uintptr_t>(fused.sh_registers) == reinterpret_cast<std::uintptr_t>(scratch), "fused registers are not in the caller's memory");
    const auto* backLo = find(fused, 0x088u);
    const auto* frontLo = find(fused, 0x0C8u);
    check(backLo != nullptr && backLo->value == static_cast<std::uint32_t>((base + 256) >> 8u), "back program address was not patched");
    check(frontLo != nullptr && frontLo->value == static_cast<std::uint32_t>(base >> 8u), "fused program address was not patched");
    check(find(fused, 0x08Au)->value == ((1u << 24u) | (1u << 6u) | 5u), "RSRC1 register counts were not merged");
    check(find(fused, 0x08Bu)->value == ((6u << 1u) | (1u << 7u) | (1u << 15u)), "RSRC2 user SGPRs were not taken from the front half");
    check(find(fused, 0x0B2u) == nullptr, "a front-only SH register was added to the fused list");
    check(find(fused, 0x080u) == nullptr, "front program checksum was kept");
    check(fused.num_sh_registers == halves.back.num_sh_registers && fused.num_cx_registers == halves.back.num_cx_registers, "fused register counts are wrong");
    check(fused.cx_registers[0].offset == 0x29Bu && fused.cx_registers[0].value == 2u, "context registers are not the back half's");
    check(fused.num_input_semantics == 2 && fused.input_semantics[1].semantic == 2, "front inputs were not kept");
    check(fused.num_output_semantics == 1 && fused.output_semantics[0].semantic == 3, "back outputs were not kept");
    check(fused.scratch_size_dw_per_thread == 16, "scratch size is not the larger half's");
    check(fused.specials != nullptr && fused.specials != halves.back.specials && fused.specials->ge_cntl.value == 0x77u, "back specials were not copied");
    const auto inside = [&](std::uintptr_t address) {
        const auto begin = reinterpret_cast<std::uintptr_t>(storage.data());
        return address >= begin && address < begin + storage.size();
    };
    const auto program = [&](const void* pointer) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        return !inside(address) && address >= base + fused.shader_size && address < base + fused.shader_size + fused.header_size + alignof(Shader);
    };
    check(!inside(base) && program(fused.cx_registers) && program(fused.specials) && program(fused.input_semantics) && program(fused.output_semantics), "fused tables are not beside the fused code");
    check(program(fused.user_data) && program(fused.user_data->direct_resource_offset) && program(fused.user_data->sharp_resource_offset[1]), "fused user data is not beside the fused code");
    check(fused.user_data->direct_resource_count == 2 && fused.user_data->direct_resource_offset[1] == 9u && fused.user_data->sharp_resource_offset[1][0].offset_dw == 12 && fused.user_data->sharp_resource_offset[0] == nullptr && fused.user_data->eud_size_dw == 4, "fused user data is wrong");
}

void testScratchReuse() {
    Halves first;
    Halves second;
    second.backCode[0] = VMovV0;
    SizeAlign size{};
    check(sceAgcGetFusedShaderSize_0080(&size, &first.front, &first.back) == 0, "fused size query failed");
    std::vector<std::uint8_t> scratch(size.m_size);
    Shader firstFused{};
    Shader secondFused{};
    check(sceAgcFuseShaderHalves_0200(&firstFused, &first.front, &first.back, scratch.data()) == 0, "first fusion failed");
    const auto firstCode = reinterpret_cast<std::uintptr_t>(firstFused.code);
    check(sceAgcFuseShaderHalves_0200(&secondFused, &second.front, &second.back, scratch.data()) == 0, "second fusion failed");
    const auto secondCode = reinterpret_cast<std::uintptr_t>(secondFused.code);
    check(firstCode != secondCode, "two fused programs share one code address");
    check(word(reinterpret_cast<const void*>(firstCode), 64) == SMovS0, "fusing into reused memory overwrote the first fused program");
    check(word(reinterpret_cast<const void*>(secondCode), 64) == VMovV0, "second fused program has the wrong back half");
    Shader again{};
    check(sceAgcFuseShaderHalves_0200(&again, &first.front, &first.back, scratch.data()) == 0, "repeated fusion failed");
    check(reinterpret_cast<std::uintptr_t>(again.code) == firstCode, "the same halves were fused into a second program");
    first.backCode[0] = VMovV0;
    Shader changed{};
    check(sceAgcFuseShaderHalves_0200(&changed, &first.front, &first.back, scratch.data()) == 0, "fusion of changed halves failed");
    check(reinterpret_cast<std::uintptr_t>(changed.code) != firstCode && word(reinterpret_cast<const void*>(firstCode), 64) == SMovS0, "changed halves reused the earlier fused program");
}

void testOffchipLdsEnable() {
    constexpr std::uint32_t offchip = 1u << 18u;
    for (const auto frontEnable : {0u, offchip}) {
        for (const auto backEnable : {0u, offchip}) {
            Halves halves;
            halves.frontSh[2].value |= frontEnable;
            halves.backSh[3].value |= backEnable;
            SizeAlign size{};
            check(sceAgcGetFusedShaderSize_0080(&size, &halves.front, &halves.back) == 0, "offchip LDS enable rejected by the size query");
            std::vector<std::uint8_t> storage(size.m_size + 512, 0xcd);
            Shader fused{};
            check(sceAgcFuseShaderHalves_0200(&fused, &halves.front, &halves.back, aligned(storage, 256)) == 0, "offchip LDS enable rejected by fusion");
            const auto* resource = find(fused, 0x08bu);
            check(resource != nullptr && resource->value == ((6u << 1u) | (1u << 7u) | (1u << 15u) | frontEnable), "fusion did not take offchip LDS enable and user SGPRs from the front half");
        }
    }
    Halves reported;
    reported.frontSh[2].value = 0x00070004u;
    reported.backSh[3].value = 0x00ab0020u;
    SizeAlign size{};
    check(sceAgcGetFusedShaderSize_0080(&size, &reported.front, &reported.back) == 0, "reported shader resources rejected by size query");
    std::vector<std::uint8_t> storage(size.m_size + 512, 0xcd);
    Shader fused{};
    check(sceAgcFuseShaderHalves_0200(&fused, &reported.front, &reported.back, aligned(storage, 256)) == 0, "reported shader resources rejected by fusion");
    check(find(fused, 0x08bu)->value == 0x00af0004u, "reported shader resources did not retain the front offchip LDS enable");
}

void testResourceMerging() {
    struct Case {
        std::uint32_t front1;
        std::uint32_t front2;
        std::uint32_t back1;
        std::uint32_t back2;
        std::uint32_t expected1;
        std::uint32_t expected2;
    };
    const Case cases[] = {
        {0u, 0x80000000u, 0u, 0u, 0u, 0x10000000u},
        {1u, 0xf0000000u, 3u, 0u, 3u, 0x10000000u},
        {0u, 0xf0000000u, 0u, 0xf0000000u, 0u, 0u},
        {0x400003c1u, 0x0006000cu, 0x61000043u, 0x00018001u, 0x61000043u, 0x0006800du},
        {0x60000007u, 0x00010000u, 0x21000041u, 0x00020000u, 0x61000047u, 0x00020000u}
    };
    for (const auto& entry : cases) {
        Halves halves;
        halves.frontSh[1].value = entry.front1;
        halves.frontSh[2].value = entry.front2;
        halves.backSh[2].value = entry.back1;
        halves.backSh[3].value = entry.back2;
        SizeAlign size{};
        check(sceAgcGetFusedShaderSize_0080(&size, &halves.front, &halves.back) == 0, "resource merge size query failed");
        std::vector<std::uint8_t> scratch(size.m_size);
        Shader fused{};
        check(sceAgcFuseShaderHalves_0200(&fused, &halves.front, &halves.back, scratch.data()) == 0, "resource merge failed");
        check(find(fused, 0x08au)->value == entry.expected1, "resource register 1 merge mismatch");
        check(find(fused, 0x08bu)->value == entry.expected2, "resource register 2 merge mismatch");
    }
}

void testRejections() {
    Halves halves;
    SizeAlign size{};
    Shader fused{};
    expectFailure([&] { sceAgcGetFusedShaderSize_0080(nullptr, &halves.front, &halves.back); });
    expectFailure([&] { sceAgcGetFusedShaderSize(nullptr, &halves.front, &halves.back); });
    expectFailure([&] { sceAgcFuseShaderHalves_0200(&fused, &halves.front, &halves.back, nullptr); });
    {
        Halves swapped;
        check(sceAgcGetFusedShaderSize_0080(&size, &swapped.back, &swapped.front) == InvalidShaderHalves, "swapped halves were accepted by the size query");
        check(sceAgcGetFusedShaderSize(nullptr, &swapped.back, &swapped.front) == InvalidShaderHalves, "the unversioned size query checked its output before the halves");
        check(sceAgcFuseShaderHalves_0200(&fused, &swapped.back, &swapped.front, nullptr) == InvalidShaderHalves, "swapped halves were fused");
    }
    {
        Halves stages;
        stages.backSpecials.vgt_shader_stages_en.value = 0;
        std::vector<std::uint8_t> storage(4096);
        check(sceAgcFuseShaderHalves_0200(&fused, &stages.front, &stages.back, storage.data()) == InvalidShaderHalves, "halves with different GS stages were fused");
    }
    {
        Halves noJump;
        noJump.frontCode[2] = SNop;
        expectFailure([&] { sceAgcGetFusedShaderSize_0080(&size, &noJump.front, &noJump.back); });
    }
    {
        Halves noTrailer;
        noTrailer.frontCode[5] = 0;
        expectFailure([&] { sceAgcGetFusedShaderSize_0080(&size, &noTrailer.front, &noTrailer.back); });
    }
}

void setFrontConstants(Halves& halves, std::uint32_t seed, std::uint32_t constantsDqw = 2, bool addAfterGetpc = true) {
    halves.frontCode = {SMovS0, SGetpcS60, addAfterGetpc ? SAddS60Literal : SMovS0, 24u, SAddcS61Zero, VMovV0, SSetpcS6, SCodeEnd};
    const auto codeBytes = static_cast<std::uint32_t>(halves.frontCode.size() * 4);
    for (std::uint32_t i = 0; i < 8; ++i) halves.frontCode.push_back(seed + i);
    std::array<std::uint32_t, 2> magic{};
    std::memcpy(magic.data(), "barefoot", 8);
    halves.frontCode.insert(halves.frontCode.end(), {magic[0], magic[1], 0xabcdu, 0u, 1u, codeBytes, 0u});
    halves.front.code = halves.frontCode.data();
    halves.front.shader_size = static_cast<std::uint32_t>(halves.frontCode.size() * 4);
    halves.front.embedded_constant_buffer_size_dqw = constantsDqw;
}

void testFrontConstantBuffer() {
    Halves halves;
    setFrontConstants(halves, 0x11110000u);
    SizeAlign size{};
    check(sceAgcGetFusedShaderSize_0080(&size, &halves.front, &halves.back) == 0, "size query refused a front half with an embedded constant buffer");
    std::vector<std::uint8_t> scratch(size.m_size);
    Shader fused{};
    check(sceAgcFuseShaderHalves_0200(&fused, &halves.front, &halves.back, scratch.data()) == 0, "fusion refused a front half with an embedded constant buffer");
    const auto* memory = static_cast<const std::uint8_t*>(const_cast<const void*>(fused.code));
    check(word(memory, 0) == SMovS0 && word(memory, 1) == SGetpcS60 && word(memory, 2) == SAddS60Literal && word(memory, 4) == SAddcS61Zero && word(memory, 5) == VMovV0, "front program was not copied");
    for (std::size_t i = 6; i < 64; ++i) check(word(memory, i) == SNop, "front program does not fall through to the back half");
    check(std::memcmp(memory + 256, halves.backCode.data(), halves.back.shader_size) == 0, "back program was not copied");
    check(fused.shader_size == 272 + 32, "fused code does not end after the moved constants");
    for (std::uint32_t i = 0; i < 8; ++i) check(word(memory, 68 + i) == 0x11110000u + i, "front constants were not moved after the back program");
    check(8 + word(memory, 3) == 272, "front s_getpc_b64 offset does not point at the moved constants");
    check(halves.frontCode[3] == 24u, "fusion changed the title's front half");
    Halves other;
    setFrontConstants(other, 0x22220000u);
    Shader otherFused{};
    check(sceAgcFuseShaderHalves_0200(&otherFused, &other.front, &other.back, scratch.data()) == 0, "second fusion failed");
    const auto* otherMemory = static_cast<const std::uint8_t*>(const_cast<const void*>(otherFused.code));
    check(otherMemory != memory && word(otherMemory, 68) == 0x22220000u && word(memory, 68) == 0x11110000u, "halves that differ only in front constants share a fused program");
    Halves overrun;
    setFrontConstants(overrun, 0x33330000u, 3);
    expectFailure([&] { sceAgcGetFusedShaderSize_0080(&size, &overrun.front, &overrun.back); });
    Halves unpatched;
    setFrontConstants(unpatched, 0x44440000u, 2, false);
    expectFailure([&] { sceAgcGetFusedShaderSize_0080(&size, &unpatched.front, &unpatched.back); });
    Halves gap;
    setFrontConstants(gap, 0x66660000u);
    gap.frontCode[3] = 20u;
    expectFailure([&] { sceAgcGetFusedShaderSize_0080(&size, &gap.front, &gap.back); });
    Halves literal;
    setFrontConstants(literal, 0x55550000u);
    literal.frontCode[0] = SMovS0Literal;
    Shader literalFused{};
    check(sceAgcFuseShaderHalves_0200(&literalFused, &literal.front, &literal.back, scratch.data()) == 0, "fusion refused a front half whose literal looks like s_getpc_b64");
    const auto* literalMemory = static_cast<const std::uint8_t*>(const_cast<const void*>(literalFused.code));
    check(word(literalMemory, 0) == SMovS0Literal && word(literalMemory, 1) == SGetpcS60 && word(literalMemory, 3) == 24u, "a literal that looks like s_getpc_b64 was relocated as one");
}

constexpr std::uint32_t ConstantLoadThreads = 32;
constexpr std::uint32_t FrontConstantSeed = 0x12340000u;
constexpr std::uint32_t BackConstantSeed = 0x56780000u;
constexpr std::uint32_t FrontImmediate = 0x24681357u;
alignas(256) std::array<std::uint32_t, ConstantLoadThreads * 2 + 8> ConstantLoadOutput{};

std::size_t appendConstants(std::vector<std::uint32_t>& code, std::uint32_t count, std::uint32_t seed) {
    const auto first = code.size();
    for (std::uint32_t i = 0; i < count * 4; ++i) code.push_back(seed + i);
    std::array<std::uint32_t, 2> magic{};
    std::memcpy(magic.data(), "barefoot", 8);
    code.insert(code.end(), {magic[0], magic[1], 0xabcdu, 0u, 1u, static_cast<std::uint32_t>(first * 4), 0u});
    return first;
}

void executeConstantLoads(AgcDriver::VulkanDevice& device, const Shader& shader, std::uint32_t front, std::uint32_t back) {
    ConstantLoadOutput.fill(0xdeadbeefu);
    const auto output = reinterpret_cast<std::uintptr_t>(ConstantLoadOutput.data());
    const std::array<std::uint32_t, 4> userData{
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu),
        static_cast<std::uint32_t>(sizeof(ConstantLoadOutput)), 0x31016facu
    };
    const auto* words = static_cast<const std::uint32_t*>(const_cast<const void*>(shader.code));
    const std::span<const std::uint32_t> code(words, shader.shader_size / 4);
    const auto address = reinterpret_cast<std::uintptr_t>(words);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{address, std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{ConstantLoadThreads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, address, code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.ComputeTarget(32),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, address);
    device.WaitIdle();
    for (std::uint32_t lane = 0; lane < ConstantLoadThreads; ++lane) {
        check(ConstantLoadOutput[lane * 2] == front, "fused front scalar constant load returned the wrong value");
        check(ConstantLoadOutput[lane * 2 + 1] == back, "fused back scalar constant load returned the wrong value");
    }
    for (std::size_t i = ConstantLoadThreads * 2; i < ConstantLoadOutput.size(); ++i) {
        check(ConstantLoadOutput[i] == 0xdeadbeefu, "fused constant loads wrote outside their output");
    }
}

void testBackConstantBuffer(AgcDriver::VulkanDevice& device, bool frontConstants, bool lastWord) {
    constexpr std::uint32_t frontDelta = 0x01000000u;
    constexpr std::uint32_t backDelta = 0x02000000u;
    const auto frontWord = lastWord ? 3u : 0u;
    const auto backWord = lastWord ? 7u : 0u;
    Shader first{};
    Shader changed{};
    std::vector<ShaderRegister> firstScratch;
    std::vector<ShaderRegister> changedScratch;
    std::vector<std::uint32_t> firstCode;
    {
        Halves halves;
        if (frontConstants) {
            halves.frontCode = {
                0x34020083u, SGetpcS60, SAddS60Literal, 44u + frontWord * 4u, SAddcS61Zero,
                0xf400031eu, 0xfa000000u, 0xbf8cc07fu, 0x7e04020cu, 0xe0701000u, 0x80000201u,
                SSetpcS6, SCodeEnd
            };
        } else {
            halves.frontCode = {0x34020083u, 0x7e0402ffu, FrontImmediate, 0xe0701000u, 0x80000201u, SSetpcS6, SCodeEnd};
        }
        const auto frontStart = appendConstants(halves.frontCode, frontConstants ? 1u : 0u, FrontConstantSeed);
        halves.front.code = halves.frontCode.data();
        halves.front.shader_size = static_cast<std::uint32_t>(halves.frontCode.size() * 4);
        halves.front.embedded_constant_buffer_size_dqw = frontConstants ? 1 : 0;
        halves.backCode = {
            SGetpcS60, SAddS60Literal, 44u + backWord * 4u, SAddcS61Zero,
            0xf400035eu, 0xfa000000u, 0xbf8cc07fu, 0x7e06020du, 0xe0701004u, 0x80000301u,
            SEndpgm, SCodeEnd
        };
        const auto backStart = appendConstants(halves.backCode, 2, BackConstantSeed);
        halves.back.code = halves.backCode.data();
        halves.back.shader_size = static_cast<std::uint32_t>(halves.backCode.size() * 4);
        halves.back.embedded_constant_buffer_size_dqw = 2;
        const auto originalFront = halves.frontCode;
        const auto originalBack = halves.backCode;
        SizeAlign size{};
        check(sceAgcGetFusedShaderSize_0080(&size, &halves.front, &halves.back) == 0, "size query refused a back constant buffer");
        check(size.m_size == halves.backSh.size() * sizeof(ShaderRegister) && size.m_align == 4, "back constants changed the caller's scratch requirement");
        firstScratch.assign(halves.backSh.size() + 1, {0xa5a5a5a5u, 0x5a5a5a5au});
        changedScratch = firstScratch;
        check(sceAgcFuseShaderHalves_0200(&first, &halves.front, &halves.back, firstScratch.data()) == 0, "fusion refused a back constant buffer");
        check(firstScratch.back().offset == 0xa5a5a5a5u && firstScratch.back().value == 0x5a5a5a5au, "back constants exceeded the caller's scratch requirement");
        check(first.embedded_constant_buffer_size_dqw == 2, "fusion did not keep the back constant buffer metadata");
        const auto* memory = static_cast<const std::uint32_t*>(const_cast<const void*>(first.code));
        check(std::memcmp(memory + 64, originalBack.data(), originalBack.size() * 4) == 0, "fusion changed or truncated the copied back program and constants");
        firstCode.assign(memory, memory + first.shader_size / 4);
        check(halves.frontCode == originalFront && halves.backCode == originalBack, "fusion changed the input programs or constant buffers");
        Shader repeated{};
        std::vector<ShaderRegister> repeatedScratch(halves.backSh.size());
        check(sceAgcFuseShaderHalves_0200(&repeated, &halves.front, &halves.back, repeatedScratch.data()) == 0 && repeated.code == first.code, "unchanged constant buffers missed the fusion cache");
        if (frontConstants) halves.frontCode[frontStart + frontWord] += frontDelta;
        halves.backCode[backStart + backWord] += backDelta;
        check(sceAgcFuseShaderHalves_0200(&changed, &halves.front, &halves.back, changedScratch.data()) == 0, "fusion refused changed constants");
        check(changed.code != first.code, "changed constant buffers reused the earlier fused program");
        check(std::memcmp(const_cast<const void*>(first.code), firstCode.data(), firstCode.size() * 4) == 0, "changing source constants changed an earlier fused program");
    }
    check(std::memcmp(const_cast<const void*>(first.code), firstCode.data(), firstCode.size() * 4) == 0, "destroying the source halves changed the fused program");
    const auto frontExpected = frontConstants ? FrontConstantSeed + frontWord : FrontImmediate;
    executeConstantLoads(device, first, frontExpected, BackConstantSeed + backWord);
    executeConstantLoads(device, changed, frontExpected + (frontConstants ? frontDelta : 0u), BackConstantSeed + backWord + backDelta);
}

void testHullHalves() {
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
    check(sceAgcGetFusedShaderSize_0080(&size, &front, &back) == 0 && size.m_size == backSh.size() * sizeof(ShaderRegister) && size.m_align == 4, "hull size query changed");
    std::vector<ShaderRegister> scratch(backSh.size());
    Shader fused{};
    check(sceAgcFuseShaderHalves_0200(&fused, &front, &back, scratch.data()) == 0, "hull fusion failed");
    check(fused.type == static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::Hs) && fused.code == back.code && fused.sh_registers == scratch.data() && fused.user_data == nullptr, "hull halves were not fused at the register level");
    const auto address = reinterpret_cast<std::uint64_t>(front.code);
    check(scratch[0].value == 0x11u && scratch[1].value == 0x22u, "hull checksums were not taken from the front half");
    check(scratch[2].value == 7u && scratch[3].value == (6u << 1u), "hull resource registers were not merged");
    check(scratch[4].value == static_cast<std::uint32_t>(address >> 8u) && scratch[5].value == (0xab00u | static_cast<std::uint32_t>((address >> 40u) & 0xffu)), "hull local program address does not point at the front half");
    check(backSh[4].value == 0u, "hull fusion wrote the back half's registers");
    const auto backBefore = backSh;
    Shader refused{};
    expectFailure([&] { sceAgcFuseShaderHalves_0200(&refused, &front, &back, nullptr); });
    check(std::memcmp(backSh.data(), backBefore.data(), sizeof(backSh)) == 0, "hull fusion without scratch wrote the back half");
    Shader noFrontSpecials = front;
    noFrontSpecials.specials = nullptr;
    expectFailure([&] { sceAgcFuseShaderHalves_0200(&refused, &noFrontSpecials, &back, scratch.data()); });
    Shader noBackSpecials = back;
    noBackSpecials.specials = nullptr;
    expectFailure([&] { sceAgcFuseShaderHalves_0200(&refused, &front, &noBackSpecials, scratch.data()); });
}

void testUnversionedSize() {
    Halves halves;
    SizeAlign size{};
    check(sceAgcGetFusedShaderSize(&size, &halves.front, &halves.back) == 0, "unversioned fused size query failed");
    check(size.m_size == halves.back.num_sh_registers * sizeof(ShaderRegister) && size.m_align == 4, "unversioned fused size did not cover the back half's register list");
    SizeAlign merged{};
    check(sceAgcGetFusedShaderSize_0080(&merged, &halves.front, &halves.back) == 0 && merged.m_size == size.m_size, "unversioned size differed from the _0080 merged geometry size");
    check(sceAgcGetFusedShaderSize(&size, &halves.back, &halves.front) == InvalidShaderHalves, "unversioned query accepted swapped halves");
    expectFailure([&] { sceAgcGetFusedShaderSize(nullptr, &halves.front, &halves.back); });
    halves.front.type = static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::HsFront);
    check(sceAgcGetFusedShaderSize(&size, &halves.front, &halves.back) == InvalidShaderHalves, "unversioned query accepted mismatched stage halves");
    halves.back.type = static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::HsBack);
    check(sceAgcGetFusedShaderSize(&size, &halves.front, &halves.back) == 0 && size.m_size == halves.back.num_sh_registers * sizeof(ShaderRegister), "unversioned query rejected hull halves");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        testFusion(0);
        testFusion(0xa0);
        testFusion(4);
        testFusion(0, SSwappcNullS6);
        testScratchReuse();
        testOffchipLdsEnable();
        testResourceMerging();
        testRejections();
        testFrontConstantBuffer();
        for (const bool frontConstants : {false, true}) {
            for (const bool lastWord : {false, true}) testBackConstantBuffer(*device, frontConstants, lastWord);
        }
        testHullHalves();
        testUnversionedSize();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC shader fusion tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
