#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 8;
alignas(256) std::array<std::uint32_t, Threads * Words> Input{};
alignas(256) std::array<std::uint32_t, Threads * 2> Output{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x34020085u, 0xe0381000u, 0x80000401u, 0xe0381010u, 0x80000801u, 0xbf8c3f70u, 0x7d8a1480u, 0xd570000cu,
    0x04220d04u, 0x34020083u, 0xe0741000u, 0x80010c01u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, 14> RuntimeCode{};

constexpr std::array<std::uint32_t, Threads * Words> Values{
    0xa676b6b3u, 0x80277947u, 0x525bd27eu, 0x000fc73du, 0xb8f0ac8cu, 0x9bcaa1e9u, 0x00000000u, 0x00000000u,
    0xa30df247u, 0xcb611bffu, 0x4559274du, 0x51aa7addu, 0x89236cadu, 0x80040cddu, 0x00000001u, 0x00000000u,
    0xdf706b4eu, 0x001d1938u, 0xe02aefceu, 0xfea5940au, 0xfd8592f3u, 0x7fd0048au, 0x00000000u, 0x00000000u,
    0x2c2df8cfu, 0xe9f22423u, 0x3f72ebe2u, 0xdd10fb58u, 0x1080e233u, 0x800e1362u, 0x00000001u, 0x00000000u,
    0x1633cd0au, 0x5fda5927u, 0xf5cde6beu, 0x80168856u, 0x5f5f2163u, 0x20028d8du, 0x00000000u, 0x00000000u,
    0xffffffffu, 0x7fefffffu, 0x056779d0u, 0x05f1de14u, 0x00000000u, 0x80000000u, 0x00000001u, 0x00000000u,
    0x6e0729aeu, 0x0019abdau, 0xfc991f6fu, 0xffda955au, 0x28856cb6u, 0x001abd04u, 0x00000000u, 0x00000000u,
    0x253bc796u, 0x800e59eau, 0x3095c4b8u, 0x5016ee85u, 0x18d2e840u, 0x000a5a70u, 0x00000001u, 0x00000000u,
    0xd72825ccu, 0xffdd08f6u, 0x77f2799fu, 0xf46679b0u, 0xcbdf1004u, 0x4b72af52u, 0x00000000u, 0x00000000u,
    0x7c038f09u, 0x001c006cu, 0xc2376325u, 0x80136a8eu, 0xfa4fe313u, 0x7fe87bf8u, 0x00000001u, 0x00000000u,
    0x14c18282u, 0xffd79e23u, 0x205fdebau, 0x477e77e3u, 0x7348f3e2u, 0xc959a519u, 0x00000000u, 0x00000000u,
    0x626a9975u, 0xa0ed783eu, 0x2071a588u, 0xa2eec7ebu, 0x1fe99688u, 0xffe81345u, 0x00000001u, 0x00000000u,
    0xb02cc2afu, 0xffd80515u, 0x00000001u, 0x00000000u, 0xb02cc2afu, 0x3cb80515u, 0x00000000u, 0x00000000u,
    0x0bb1cea1u, 0x7fdde69au, 0x00000001u, 0x80000000u, 0x0bb1cea1u, 0x3cbde69au, 0x00000001u, 0x00000000u,
    0x730bd71au, 0xffd8e84eu, 0x4e9ec55au, 0x7fec12b5u, 0x00000001u, 0x80000000u, 0x00000001u, 0x00000000u,
    0x9c9d53ccu, 0x001d4894u, 0x5a6adcc6u, 0x801ce0f4u, 0x729337f0u, 0xffd36846u, 0x00000001u, 0x00000000u,
    0xe6e94003u, 0x7fdffaaau, 0xb8556e0bu, 0xffd802b0u, 0xffffffffu, 0xffefffffu, 0x00000000u, 0x00000000u,
    0xe78175f1u, 0x03c97d58u, 0x51a80c01u, 0x7552ceecu, 0x1943cd8au, 0xbc16929du, 0x00000000u, 0x00000000u,
    0x3a6dce08u, 0x76c606f9u, 0xda8ac91fu, 0x001d057fu, 0x94209e49u, 0xb5ec0ebfu, 0x00000000u, 0x00000000u,
    0xccee6edfu, 0x800b0840u, 0x30f7a2b5u, 0xeaea3654u, 0xab274b6du, 0x7fd5be8eu, 0x00000001u, 0x00000000u,
    0x83f77e40u, 0xe5c7bc3du, 0x36a3b92bu, 0x16832a8bu, 0x3815f978u, 0xffd76199u, 0x00000000u, 0x00000000u,
    0xab968516u, 0xffebc3fdu, 0x4afe2211u, 0x0792c9aau, 0x2f7c727fu, 0x5757a0dbu, 0x00000000u, 0x00000000u,
    0xe02bf95bu, 0x001f143du, 0x10be1a1bu, 0x7fd1d0cbu, 0xa9f3c0a2u, 0xc0014d89u, 0x00000001u, 0x00000000u,
    0xead9a11du, 0xd616c68bu, 0xcc2a3be5u, 0x80219004u, 0x690b734au, 0x9648fff6u, 0x00000000u, 0x00000000u,
    0x7095b2d3u, 0x00084916u, 0xba60938eu, 0x9896dbbfu, 0xb00f63aau, 0xffe76996u, 0x00000000u, 0x00000000u,
    0x692ef2d6u, 0x802ec61bu, 0x81d2c69eu, 0x7fec7ae5u, 0xfb1792a2u, 0x402b6387u, 0x00000000u, 0x00000000u,
    0x3ab6a70fu, 0xfd0d12e7u, 0x5451e547u, 0x00181347u, 0xffffffffu, 0x7fefffffu, 0x00000001u, 0x00000000u,
    0xeb66a2e0u, 0xe4e64812u, 0x2af0ad3cu, 0xffeb2e77u, 0xf3b1c040u, 0x72ce7dc6u, 0x00000000u, 0x00000000u,
    0x47e175f9u, 0x71608ce5u, 0xc52f6fd7u, 0x57f3285cu, 0xcb4ef484u, 0xffd76041u, 0x00000000u, 0x00000000u,
    0x8ec37699u, 0x7d081b1cu, 0x1e772796u, 0x39cc7ff0u, 0x264c68fau, 0x8002d00au, 0x00000001u, 0x00000000u,
    0x00000000u, 0x3ff00000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0xbff00000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x3ff00000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0xbff00000u, 0x00000001u, 0x00000000u
};

constexpr std::uint64_t Expected[3][Threads] = {
    {
        0x9bcaa1e9b8f0ac8cull, 0xd51c50e18315046full, 0x7fd0048afd8592f3ull, 0x7f13411e22a51467ull,
        0x1d1bf0abe4e1535bull, 0x3df1de14056779d0ull, 0xc0055372a870a7b1ull, 0x88349192e51a755eull,
        0x7ff0000000000000ull, 0x7ff0000000000000ull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x0000000000000000ull, 0x0000000000000000ull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0xffefffffffffffffull, 0xbc16929d1943cd4eull, 0x36f3fa0ceea10415ull, 0x7ff0000000000000ull,
        0xffd761993815f978ull, 0x5757a0db2f7c727full, 0x451e20dab376022bull, 0x9365c174ace1bcc8ull,
        0xffe76996b00f63aaull, 0x3d5b6a616bbba89cull, 0x7ff0000000000000ull, 0x7ff0000000000000ull,
        0x7ff0000000000000ull, 0x6ee57819788afba9ull, 0x0000000000000000ull, 0x0000000000000000ull
    },
    {
        0x9bcaa1e9b8f0ac8dull, 0xd51c50e183150470ull, 0x7fd0048afd8592f2ull, 0x7f13411e22a51466ull,
        0x1d1bf0abe4e1535aull, 0x3df1de14056779cfull, 0xc0055372a870a7b2ull, 0x88349192e51a755full,
        0x7fefffffffffffffull, 0x7fefffffffffffffull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0x8000000000000000ull, 0x8000000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0xfff0000000000000ull, 0xbc16929d1943cd4full, 0x36f3fa0ceea10414ull, 0x7fefffffffffffffull,
        0xffd761993815f979ull, 0x5757a0db2f7c727eull, 0x451e20dab376022aull, 0x9365c174ace1bcc9ull,
        0xffe76996b00f63abull, 0x3d5b6a616bbba89bull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
        0x7fefffffffffffffull, 0x6ee57819788afba8ull, 0x8000000000000000ull, 0x8000000000000000ull
    },
    {
        0x9bcaa1e9b8f0ac8cull, 0xd51c50e18315046full, 0x7fd0048afd8592f2ull, 0x7f13411e22a51466ull,
        0x1d1bf0abe4e1535aull, 0x3df1de14056779cfull, 0xc0055372a870a7b1ull, 0x88349192e51a755eull,
        0x7fefffffffffffffull, 0x7fefffffffffffffull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x0000000000000000ull, 0x0000000000000000ull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0xffefffffffffffffull, 0xbc16929d1943cd4eull, 0x36f3fa0ceea10414ull, 0x7fefffffffffffffull,
        0xffd761993815f978ull, 0x5757a0db2f7c727eull, 0x451e20dab376022aull, 0x9365c174ace1bcc8ull,
        0xffe76996b00f63aaull, 0x3d5b6a616bbba89bull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
        0x7fefffffffffffffull, 0x6ee57819788afba8ull, 0x0000000000000000ull, 0x0000000000000000ull
    }
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

std::optional<std::string> Run(AgcDriver::VulkanDevice& device, ShaderRecompiler::ShaderFloatMode floatMode, std::span<const std::uint32_t> code) {
    Input = Values;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    std::optional<decltype(ShaderRecompiler::Recompile(request))> result;
    try {
        result.emplace(ShaderRecompiler::Recompile(request));
    } catch (const std::exception& error) {
        return std::string(error.what());
    }
    device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return std::nullopt;
}

void Check(std::uint32_t rounding, const std::string& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Values[tid * Words];
        const auto wide = [&](std::uint32_t index) { return in[index] | static_cast<std::uint64_t>(in[index + 1u]) << 32u; };
        const std::uint64_t actual = Output[tid * 2u] | static_cast<std::uint64_t>(Output[tid * 2u + 1u]) << 32u;
        const std::uint64_t expected = Expected[rounding - 1u][tid];
        Require(actual == expected, "v_div_fmas_f64 " + mode + " of " + Hex(wide(0u)) + ", " + Hex(wide(2u)) + ", " + Hex(wide(4u)) + (in[6] != 0u ? " with VCC" : "") + " is " + Hex(actual) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFloat64)) {
            std::puts("skipped, the device has no shaderFloat64");
            return VulkanTestSkipped;
        }
        std::uint32_t checked = 0u;
        for (std::uint32_t rounding = 1u; rounding < 4u; ++rounding) {
            for (const std::uint32_t denormals : {0xc0u, 0x00u}) {
                for (const bool ieee : {false, true}) {
                    const std::uint32_t field = rounding << 2u;
                    const std::string declared = "FLOAT_MODE " + Hex(denormals | field) + (ieee ? ", IEEE=1" : ", IEEE=0");
                    const auto refusal = Run(*device, ShaderRecompiler::ShaderFloatMode{denormals | field, true, ieee, false}, Code);
                    Require(!refusal, declared + ": " + refusal.value_or(""));
                    Check(rounding, declared);
                    RuntimeCode[0] = 0xbfa40000u | field;
                    std::copy(Code.begin(), Code.end(), RuntimeCode.begin() + 1);
                    const auto runtimeRefusal = Run(*device, ShaderRecompiler::ShaderFloatMode{denormals, true, ieee, false}, RuntimeCode);
                    Require(!runtimeRefusal, "s_round_mode " + Hex(field) + " after " + declared + ": " + runtimeRefusal.value_or(""));
                    Check(rounding, "s_round_mode " + Hex(field) + " after FLOAT_MODE " + Hex(denormals) + (ieee ? ", IEEE=1" : ", IEEE=0"));
                    checked += 2u;
                }
            }
        }
        std::printf("v_div_fmas_f64 directed rounding tests passed (%u of 24 runs, declared and set by s_round_mode)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
