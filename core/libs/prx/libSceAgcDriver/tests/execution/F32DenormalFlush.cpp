#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 37> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x06140b04u, 0x08160905u, 0x10180b04u,
    0x0e1a0b04u, 0xd54b000eu, 0x041a0b04u, 0x7e1e0306u, 0x561e0b04u, 0x1e200b04u, 0xd5540011u, 0x041a0b04u,
    0xd5570012u, 0x041a0b04u, 0x7e264904u, 0x7e284505u, 0x7e2a8104u, 0x7c020b04u, 0xd5010016u, 0x01a90280u,
    0x7e2e1b04u, 0x7e307f05u, 0xd5080119u, 0x42020b04u, 0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u,
    0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u, 0xbf810000u,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000001u, 0x3f800000u, 0x00000000u, 0x00000000u},
    {0x80400000u, 0x3f800000u, 0x00800000u, 0x00000000u},
    {0x3f800000u, 0x00012345u, 0x80000001u, 0x00000000u},
    {0x007fffffu, 0x007fffffu, 0x3f800000u, 0x00000000u},
    {0x3f7fffffu, 0x00800000u, 0x00000000u, 0x00000000u},
    {0xbf7fffffu, 0x00800000u, 0x80000000u, 0x00000000u},
    {0x3f7fffffu, 0x00800001u, 0x00000001u, 0x00000000u},
    {0x3f000000u, 0x01000000u, 0x80000001u, 0x00000000u},
    {0x00800000u, 0x80800001u, 0x00800000u, 0x00000000u},
    {0x00800001u, 0x00800000u, 0x3f800000u, 0x00000000u},
    {0x00000000u, 0x80000001u, 0x00000001u, 0x00000000u},
    {0x80000000u, 0x00000001u, 0x807fffffu, 0x00000000u},
    {0x7f800000u, 0x00000001u, 0x3f800000u, 0x00000000u},
    {0x00000001u, 0xff800000u, 0x00000000u, 0x00000000u},
    {0x7fc00000u, 0x00000001u, 0x00000001u, 0x00000000u},
    {0x40000000u, 0x00400000u, 0x80800000u, 0x00000000u},
    {0x3f800000u, 0x3f800000u, 0x80000001u, 0x00000000u},
    {0xbf800000u, 0x00000001u, 0x3f000000u, 0x00000000u},
    {0x34000000u, 0x34000000u, 0x00000000u, 0x00000000u},
    {0x1f800000u, 0x1f800000u, 0x00000000u, 0x00000000u},
    {0x1fffffffu, 0x1fffffffu, 0x00800000u, 0x00000000u},
    {0x20000000u, 0x1f000000u, 0x00000000u, 0x00000000u},
    {0x20000000u, 0x1f7fffffu, 0x00000000u, 0x00000000u},
    {0x20000001u, 0x1f7fffffu, 0x00000000u, 0x00000000u},
    {0xa0000000u, 0x1f7fffffu, 0x80000000u, 0x00000000u},
    {0x3f800000u, 0xbf800000u, 0x00000001u, 0x00000000u},
    {0x00800000u, 0x3f000000u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x3f7fffffu, 0x00000001u, 0x00000000u},
    {0x41200000u, 0x00000003u, 0x3dcccccdu, 0x00000000u},
    {0xc0490fdbu, 0x80000002u, 0x40490fdbu, 0x00000000u},
    {0x80000001u, 0x80000001u, 0x80000001u, 0x00000000u},
    {0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u}
};
constexpr std::uint32_t Expected[32][16] = {
    {0x3f800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x80000000u},
    {0x3f800000u, 0x3f800000u, 0x80000000u, 0x00000000u, 0x00800000u, 0x00800000u, 0x80000000u, 0x3f800000u, 0x00800000u, 0x80000000u, 0x3f800000u, 0x80000000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x80000000u},
    {0x3f800000u, 0xbf800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x3f000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x3f7fffffu, 0xbf7fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00800000u, 0x3f7fffffu, 0x00800000u, 0x00000000u, 0x3f800000u, 0x3f7fffffu, 0x00000000u, 0x00000000u, 0xffffff83u, 0x80000000u},
    {0xbf7fffffu, 0x3f7fffffu, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0xbf7fffffu, 0x00800000u, 0x80000000u, 0xbf800000u, 0x3f800000u, 0xbf7fffffu, 0x00000001u, 0xffffffffu, 0xffffff83u, 0x80000000u},
    {0x3f7fffffu, 0xbf7fffffu, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800001u, 0x3f7fffffu, 0x00800001u, 0x00000000u, 0x3f800000u, 0x3f7fffffu, 0x00000000u, 0x00000000u, 0xffffff83u, 0x80800000u},
    {0x3f000000u, 0xbf000000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x01000000u, 0x3f000000u, 0x01000000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffff84u, 0x80800000u},
    {0x80000000u, 0x81000000u, 0x80000000u, 0x80000000u, 0x00800000u, 0x00800000u, 0x80800001u, 0x00800000u, 0x00800000u, 0x00000000u, 0x80000000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffff83u, 0x00000000u},
    {0x01000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00800000u, 0x3f800000u, 0x00800001u, 0x00000000u, 0x3f800000u, 0x3f000001u, 0x00000000u, 0x00000000u, 0xffffff83u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7f800000u, 0xff800000u, 0xffc00000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0x00000000u, 0x7f800000u, 0x3f800000u, 0x7f800000u, 0x00000000u, 0x7f800000u, 0x00000000u, 0x7fffffffu, 0x00000000u, 0xffc00000u},
    {0xff800000u, 0xff800000u, 0xffc00000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0xff800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xff800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffc00000u},
    {0x7fc00000u, 0xffc00000u, 0x7fc00000u, 0x00000000u, 0x7fc00000u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7fc00000u, 0x00000000u, 0x7fc00000u, 0x00000000u, 0x7fffffffu, 0x00000000u, 0x7fc00000u},
    {0x40000000u, 0xc0000000u, 0x00000000u, 0x00000000u, 0x80800000u, 0x80800000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x3f000000u, 0x00000000u, 0x00000002u, 0x00000000u, 0x80000000u},
    {0x40000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0xbf800000u},
    {0xbf800000u, 0x3f800000u, 0x80000000u, 0x00000000u, 0x3f000000u, 0x3f000000u, 0xbf800000u, 0x3f000000u, 0x00000000u, 0xbf800000u, 0x00000000u, 0xbf000000u, 0x00000001u, 0xffffffffu, 0x00000000u, 0x80000000u},
    {0x34800000u, 0x00000000u, 0x28800000u, 0x28800000u, 0x28800000u, 0x28800000u, 0x34000000u, 0x34000000u, 0x34000000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffffeau, 0xa8800000u},
    {0x20000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x1f800000u, 0x1f800000u, 0x1f800000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffffc1u, 0x80000000u},
    {0x207fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00ffffffu, 0x00ffffffu, 0x1fffffffu, 0x1fffffffu, 0x1fffffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu, 0x00000000u, 0x00000000u, 0xffffffc1u, 0x80000000u},
    {0x20200000u, 0x9fc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x1f000000u, 0x20000000u, 0x1f000000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffffc0u, 0x80000000u},
    {0x20400000u, 0x9f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x1f7fffffu, 0x20000000u, 0x1f7fffffu, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffffc0u, 0x80000000u},
    {0x20400001u, 0x9f800002u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x1f7fffffu, 0x20000001u, 0x1f7fffffu, 0x00000000u, 0x3f800000u, 0x3f000001u, 0x00000000u, 0x00000000u, 0xffffffc0u, 0x80000000u},
    {0x9f800000u, 0x20400000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0xa0000000u, 0x1f7fffffu, 0x80000000u, 0xbf800000u, 0x3f800000u, 0xbf000000u, 0x00000001u, 0xffffffffu, 0xffffffc0u, 0x80000000u},
    {0x00000000u, 0xc0000000u, 0xbf800000u, 0xbf800000u, 0xbf800000u, 0xbf800000u, 0xbf800000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0xbf800000u, 0x3f000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x3f800000u},
    {0x3f000000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00800000u, 0x3f000000u, 0x00800000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x3f7fffffu, 0x3f7fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00800000u, 0x3f7fffffu, 0x00800000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x41200000u, 0xc1200000u, 0x00000000u, 0x00000000u, 0x3dcccccdu, 0x3dcccccdu, 0x00000000u, 0x41200000u, 0x3dcccccdu, 0x41200000u, 0x00000000u, 0x3f200000u, 0x00000000u, 0x0000000au, 0x00000000u, 0x80000000u},
    {0xc0490fdbu, 0x40490fdbu, 0x00000000u, 0x00000000u, 0x40490fdbu, 0x40490fdbu, 0xc0490fdbu, 0x40490fdbu, 0x80000000u, 0xc0800000u, 0x80000000u, 0xbf490fdbu, 0x00000001u, 0xfffffffcu, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u}
};
constexpr const char* Names[16] = {
    "v_add_f32 v10, v4, v5",
    "v_sub_f32 v11, v5, v4",
    "v_mul_f32 v12, v4, v5",
    "v_mul_legacy_f32 v13, v4, v5",
    "v_fma_f32 v14, v4, v5, v6",
    "v_fmac_f32 v15, v4, v5",
    "v_min_f32 v16, v4, v5",
    "v_max3_f32 v17, v4, v5, v6",
    "v_med3_f32 v18, v4, v5, v6",
    "v_floor_f32 v19, v4",
    "v_ceil_f32 v20, v5",
    "v_frexp_mant_f32 v21, v4",
    "v_cmp_lt_f32 v4, v5",
    "v_cvt_flr_i32_f32 v23, v4",
    "v_frexp_exp_i32_f32 v24, v5",
    "v_mul_f32 v25, |v4|, -v5"
};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), words);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("f32 denormal flush: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

bool IsNan(std::uint32_t value) {
    return (value & 0x7fffffffu) > 0x7f800000u;
}

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const char* mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < 16; ++i) {
            const bool floatResult = i < 12u || i == 15u;
            if (floatResult && IsNan(Expected[tid][i])) {
                Require(IsNan(out[i]), std::string("f32 denormal flush: lane ") + std::to_string(tid) + " " + mode + " " + Names[i] + " is " + Hex(out[i]) + ", expected a NaN");
                continue;
            }
            Expect(tid, out[i], Expected[tid][i], (std::string(mode) + " " + Names[i]).c_str());
        }
    }
}

const Testing::Case noFloatMode{"F32DenormalFlush_NoFloatMode_MatchesReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, std::nullopt);
    Check("no float mode");
}};

const Testing::Case ieeeDisabledF32Flushed{"F32DenormalFlush_IeeeDisabledF32Flushed_MatchesReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false});
    Check("IEEE=0 f32 denormals flushed");
}};

const Testing::Case ieeeEnabledAllFlushed{"F32DenormalFlush_IeeeEnabledAllFlushed_MatchesReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, ShaderRecompiler::ShaderFloatMode{0x00u, false, true, false});
    Check("IEEE=1 all denormals flushed");
}};

const Testing::Case unsupportedDenormalModes{"F32DenormalFlush_UnsupportedDenormalModes_AreRefused", [] {
    auto& device = SharedVulkanTestDevice();
    for (const std::uint32_t mode : {0xd0u, 0xe0u}) {
        const auto error = Testing::RequireThrows<std::runtime_error>([&] {
            Run(device, ShaderRecompiler::ShaderFloatMode{mode, true, false, false});
        }, "f32 denormal flush: FLOAT_MODE " + Hex(mode) + " was not refused");
        Require(std::string(error.what()).find("f32 denormal mode") != std::string::npos,
                "f32 denormal flush: FLOAT_MODE " + Hex(mode) + " was refused with an unexpected message: " + error.what());
    }
}};

} // namespace
