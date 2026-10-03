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
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 62> Code{
    0x34020084, 0x34060086, 0xe0301000, 0x80000401, 0xe0301004, 0x80000501, 0xe0301008, 0x80000601,
    0xe030100c, 0x80000701, 0xbf8c3f70, 0xe0701000, 0x80010403, 0xe0701004, 0x80010503, 0xbf8c3f70,
    0x7e500306, 0x7e520307, 0xe1705000, 0x80012803, 0xbf8c3f70, 0x7e140328, 0x7e160329, 0xe030d000,
    0x80010c03, 0xe030d004, 0x80010d03, 0xbf8c3f70, 0xe0701008, 0x80010403, 0xe070100c, 0x80010503,
    0xbf8c3f70, 0x7e500306, 0x7e520307, 0xe1745008, 0x80012803, 0xbf8c3f70, 0x7e1c0328, 0x7e1e0329,
    0xe030d008, 0x80011003, 0xe030d00c, 0x80011103, 0xbf8c3f70, 0xe0701000, 0x80010a03, 0xe0701004,
    0x80010b03, 0xe0701008, 0x80010c03, 0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03, 0xe0701014,
    0x80010f03, 0xe0701018, 0x80011003, 0xe070101c, 0x80011103, 0xbf810000,
};

constexpr const char* Names[8] = {"inc_x2_ret0", "inc_x2_ret1", "inc_x2_mem0", "inc_x2_mem1", "dec_x2_ret0", "dec_x2_ret1", "dec_x2_mem0", "dec_x2_mem1"};
constexpr std::uint32_t Rows[32][4] = {
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u},
    {0x00000002u, 0x00000000u, 0x00000001u, 0x00000000u},
    {0xffffffffu, 0x00000000u, 0xffffffffu, 0x00000000u},
    {0x00000000u, 0x00000001u, 0xffffffffu, 0x00000000u},
    {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu},
    {0x00000005u, 0x00000000u, 0x00000004u, 0x00000000u},
    {0x00000005u, 0x00000000u, 0x00000006u, 0x00000000u},
    {0x0000003cu, 0x00000000u, 0x4da4f9fcu, 0x1a6916c7u},
    {0x7a97c643u, 0x27ac435au, 0x7a97c643u, 0x27ac435au},
    {0x00000066u, 0x00000000u, 0x00000065u, 0x00000000u},
    {0xccea71ffu, 0xfd724452u, 0xccea7200u, 0xfd724452u},
    {0x00000085u, 0x00000000u, 0x00000084u, 0x00000000u},
    {0xc79d6793u, 0x2c33be0au, 0xc79d6792u, 0x2c33be0au},
    {0x00000036u, 0x00000000u, 0x00000036u, 0x00000000u},
    {0xd4341aadu, 0xa4042bb3u, 0xd4341aacu, 0xa4042bb3u},
    {0x00000031u, 0x00000000u, 0x00000030u, 0x00000000u},
    {0xa0817910u, 0xde08caa1u, 0xa081790fu, 0xde08caa1u},
    {0x00000016u, 0x00000000u, 0x00000015u, 0x00000000u},
    {0xabf4a07cu, 0x634f806fu, 0xabf4a07du, 0x634f806fu},
    {0x0000003fu, 0x00000000u, 0x0000003fu, 0x00000000u},
    {0xf1cfd992u, 0xef412ed6u, 0xf1cfd991u, 0xef412ed6u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xd9196adau, 0xc3e4a892u, 0x8224b122u, 0x31f3b923u},
    {0x0000006cu, 0x00000000u, 0x995253fdu, 0x49c7b59bu},
    {0x738d243au, 0x294c4ea3u, 0x738d2439u, 0x294c4ea3u},
    {0x000000d0u, 0x00000000u, 0x000000d0u, 0x00000000u},
    {0x0bdbc23au, 0x7671863cu, 0x0bdbc239u, 0x7671863cu},
    {0x00000084u, 0x00000000u, 0x88dcf943u, 0xa5e333cbu},
    {0xb36cc9aau, 0x57c49391u, 0xb36cc9abu, 0x57c49391u},
    {0x00000011u, 0x00000000u, 0x00000012u, 0x00000000u},
    {0xa2909cb6u, 0xa1f65507u, 0xa2909cb7u, 0xa1f65507u},
};
constexpr std::uint32_t Expected[32][8] = {
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000002u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000002u, 0x00000000u, 0x00000001u, 0x00000000u},
    {0xffffffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0x00000000u, 0xfffffffeu, 0x00000000u},
    {0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0xffffffffu, 0x00000000u},
    {0xffffffffu, 0xffffffffu, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0xfffffffeu, 0xffffffffu},
    {0x00000005u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000005u, 0x00000000u, 0x00000004u, 0x00000000u},
    {0x00000005u, 0x00000000u, 0x00000006u, 0x00000000u, 0x00000005u, 0x00000000u, 0x00000004u, 0x00000000u},
    {0x0000003cu, 0x00000000u, 0x0000003du, 0x00000000u, 0x0000003cu, 0x00000000u, 0x0000003bu, 0x00000000u},
    {0x7a97c643u, 0x27ac435au, 0x00000000u, 0x00000000u, 0x7a97c643u, 0x27ac435au, 0x7a97c642u, 0x27ac435au},
    {0x00000066u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000066u, 0x00000000u, 0x00000065u, 0x00000000u},
    {0xccea71ffu, 0xfd724452u, 0xccea7200u, 0xfd724452u, 0xccea71ffu, 0xfd724452u, 0xccea71feu, 0xfd724452u},
    {0x00000085u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000085u, 0x00000000u, 0x00000084u, 0x00000000u},
    {0xc79d6793u, 0x2c33be0au, 0x00000000u, 0x00000000u, 0xc79d6793u, 0x2c33be0au, 0xc79d6792u, 0x2c33be0au},
    {0x00000036u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000036u, 0x00000000u, 0x00000035u, 0x00000000u},
    {0xd4341aadu, 0xa4042bb3u, 0x00000000u, 0x00000000u, 0xd4341aadu, 0xa4042bb3u, 0xd4341aacu, 0xa4042bb3u},
    {0x00000031u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000031u, 0x00000000u, 0x00000030u, 0x00000000u},
    {0xa0817910u, 0xde08caa1u, 0x00000000u, 0x00000000u, 0xa0817910u, 0xde08caa1u, 0xa081790fu, 0xde08caa1u},
    {0x00000016u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000016u, 0x00000000u, 0x00000015u, 0x00000000u},
    {0xabf4a07cu, 0x634f806fu, 0xabf4a07du, 0x634f806fu, 0xabf4a07cu, 0x634f806fu, 0xabf4a07bu, 0x634f806fu},
    {0x0000003fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000003fu, 0x00000000u, 0x0000003eu, 0x00000000u},
    {0xf1cfd992u, 0xef412ed6u, 0x00000000u, 0x00000000u, 0xf1cfd992u, 0xef412ed6u, 0xf1cfd991u, 0xef412ed6u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xd9196adau, 0xc3e4a892u, 0x00000000u, 0x00000000u, 0xd9196adau, 0xc3e4a892u, 0x8224b122u, 0x31f3b923u},
    {0x0000006cu, 0x00000000u, 0x0000006du, 0x00000000u, 0x0000006cu, 0x00000000u, 0x0000006bu, 0x00000000u},
    {0x738d243au, 0x294c4ea3u, 0x00000000u, 0x00000000u, 0x738d243au, 0x294c4ea3u, 0x738d2439u, 0x294c4ea3u},
    {0x000000d0u, 0x00000000u, 0x00000000u, 0x00000000u, 0x000000d0u, 0x00000000u, 0x000000cfu, 0x00000000u},
    {0x0bdbc23au, 0x7671863cu, 0x00000000u, 0x00000000u, 0x0bdbc23au, 0x7671863cu, 0x0bdbc239u, 0x7671863cu},
    {0x00000084u, 0x00000000u, 0x00000085u, 0x00000000u, 0x00000084u, 0x00000000u, 0x00000083u, 0x00000000u},
    {0xb36cc9aau, 0x57c49391u, 0xb36cc9abu, 0x57c49391u, 0xb36cc9aau, 0x57c49391u, 0xb36cc9a9u, 0x57c49391u},
    {0x00000011u, 0x00000000u, 0x00000012u, 0x00000000u, 0x00000011u, 0x00000000u, 0x00000010u, 0x00000000u},
    {0xa2909cb6u, 0xa1f65507u, 0xa2909cb7u, 0xa1f65507u, 0xa2909cb6u, 0xa1f65507u, 0xa2909cb5u, 0xa1f65507u},
};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), words);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("buffer atomics inc dec64: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

void Run(AgcDriver::VulkanDevice& device) {
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
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t index = 0; index < 8; ++index) Expect(tid, out[index], Expected[tid][index], Names[index]);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("buffer atomics inc dec64 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
